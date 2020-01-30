#include "sys/fs/fcntl.h"
#include "sys/fs/ofile.h"
#include "sys/fs/node.h"
#include "sys/dev/blk.h"
#include "sys/dev/ram.h"
#include "sys/fs/tar.h"
#include "sys/fs/vfs.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/errno.h"
#include "sys/heap.h"
#include "sys/mm.h"

#define TAR_DIRECT_BLOCKS       40
#define TAR_INDIRECT_BLOCKS     4

struct tar {
    char filename[100];
    union {
        struct {
            char mode[8];
            char uid[8];
            char gid[8];
            char size[12];
            char mtime[12];
            char chksum[8];
            char typeflag[1];
        } __attribute__((packed)) meta_oct;
        struct {
            uint32_t mode;
            uint32_t __pad0;
            uint32_t uid;
            uint32_t __pad1;
            uint32_t gid;
            uint32_t __pad2;
            uint32_t size;
            uint64_t __pad3;
            uint32_t mtime;
            uint32_t ctime;
            // The rest is irrelevant
        } __attribute__((packed)) meta;
    };
    // 157 bytes
    // Allows to have files up to ~128KiB
    uintptr_t direct_blocks[TAR_DIRECT_BLOCKS];
    uintptr_t indirect_blocks[TAR_INDIRECT_BLOCKS];
    char __pad[3];
    // Should be 512 bytes
} __attribute__((packed));

static int tarfs_vnode_open(struct ofile *of, int opt);
static int tarfs_vnode_stat(struct vnode *node, struct stat *st);
static ssize_t tarfs_vnode_read(struct ofile *fd, void *buf, size_t count);
static ssize_t tarfs_vnode_write(struct ofile *fd, const void *buf, size_t count);
static off_t tarfs_vnode_lseek(struct ofile *fd, off_t off, int whence);
static int tarfs_vnode_chmod(struct vnode *node, mode_t mode);
static int tarfs_vnode_chown(struct vnode *node, uid_t uid, gid_t gid);
static int tarfs_vnode_mkdir(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode);
static int tarfs_vnode_creat(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode);
static int tarfs_vnode_truncate(struct vnode *node, size_t length);

static struct vnode_operations _tarfs_vnode_op = {
    .stat = tarfs_vnode_stat,
    .read = tarfs_vnode_read,
    .open = tarfs_vnode_open,
    .lseek = tarfs_vnode_lseek,
    .chmod = tarfs_vnode_chmod,
    .chown = tarfs_vnode_chown,
    .mkdir = tarfs_vnode_mkdir,
    .creat = tarfs_vnode_creat,
    .write = tarfs_vnode_write,
    .truncate = tarfs_vnode_truncate
};

static ssize_t tarfs_octal(const char *buf, size_t lim) {
    ssize_t res = 0;
    for (size_t i = 0; i < lim; ++i) {
        if (!buf[i]) {
            break;
        }

        res <<= 3;
        res |= buf[i] - '0';
    }
    return res;
}

static const char *path_element(const char *path, char *element) {
    const char *sep = strchr(path, '/');
    if (!sep) {
        strcpy(element, path);
        return NULL;
    } else {
        _assert(sep - path < NODE_MAXLEN);
        strncpy(element, path, sep - path);
        element[sep - path] = 0;

        while (*sep == '/') {
            ++sep;
        }
        if (!*sep) {
            return NULL;
        }

        return sep;
    }
}

static int tarfs_mapper_create_node(struct vnode *at, struct vnode **res, const char *name, int dir) {
    _assert(at->type == VN_DIR); // Can only attach child to a directory

    if (vnode_lookup_child(at, name, res) == 0) {
        // Already exists
        return 0;
    }

    struct vnode *node_new = vnode_create(dir ? VN_DIR : VN_REG, name);
    node_new->flags |= VN_MEMORY;
    node_new->op = &_tarfs_vnode_op;

    vnode_attach(at, node_new);
    *res = node_new;

    return 0;
}

static int tarfs_mapper_create_path(struct vnode *root, const char *path, struct vnode **res, int dir) {
    char name[NODE_MAXLEN];
    const char *child_path = path;
    struct vnode *node = root;
    struct vnode *new_node;
    int err;

    kdebug("Add path %s\n", path);

    while (1) {
        child_path = path_element(child_path, name);

        if ((err = tarfs_mapper_create_node(node, &new_node, name, dir)) != 0) {
            return err;
        }

        node = new_node;

        if (!child_path) {
            break;
        }
    }

    *res = node;

    return 0;
}

static int tar_init(struct fs *tar, const char *opt) {
    size_t off = 0;
    int prev_zero = 0;
    ssize_t res;

    // Writable tarfs is only supported if the filesystem is backed by a RAM device
    void *mem_base;
    if ((res = blk_ioctl(tar->blk, RAM_IOC_GETBASE, &mem_base)) != 0) {
        return res;
    }

    // Create filesystem root node
    struct vnode *tar_root = vnode_create(VN_DIR, NULL);
    tar->fs_private = tar_root;
    tar_root->fs = tar;
    tar_root->flags |= VN_MEMORY;
    tar_root->op = &_tarfs_vnode_op;
    tar_root->fs_data = NULL;

    tar_root->uid = 0;
    tar_root->gid = 0;
    tar_root->mode = 0555;

    struct vnode *node;

    while (1) {
        char *block = (char *) mem_base + off;
        // Check that the block is at least 2B-aligned, so its pointer can be tagged
        _assert(!(((uintptr_t) block) & 1));

        if (!block[0]) {
            if (prev_zero) {
                break;
            }

            prev_zero = 1;
            off += 512;
            continue;
        }

        struct tar *hdr = kmalloc(512);
        _assert(hdr);
        memcpy(hdr, block, 512);

        size_t node_size = 0;
        int isdir = 1;
        // Convert node metadata values from octal
        uid_t uid = tarfs_octal(hdr->meta_oct.uid, 8);
        gid_t gid = tarfs_octal(hdr->meta_oct.gid, 8);
        mode_t mode = tarfs_octal(hdr->meta_oct.mode, 8) & VFS_MODE_MASK;

        time_t mtime = tarfs_octal(hdr->meta_oct.mtime, 12);

        if (hdr->meta_oct.typeflag[0] == '\0' || hdr->meta_oct.typeflag[0] == '0') {
            node_size = tarfs_octal(hdr->meta_oct.size, 12);
            mode |= S_IFREG;
            isdir = 0;
        } else {
            mode |= S_IFDIR;
            node_size = 0;
        }

        size_t size_blocks = (node_size + 511) / 512;
        if (size_blocks >= TAR_DIRECT_BLOCKS + 64 * TAR_INDIRECT_BLOCKS) {
            panic("File is too large: %S\n", node_size);
        }

        // Map blocks
        for (size_t i = 0; i < size_blocks && i < TAR_DIRECT_BLOCKS; ++i) {
            // Tag this block pointer
            hdr->direct_blocks[i] = (uintptr_t) block + 512 + i * 512 + 1;
        }
        for (size_t i = 0; i < TAR_INDIRECT_BLOCKS; ++i) {
            hdr->indirect_blocks[i] = 0;
        }
        for (size_t i = TAR_DIRECT_BLOCKS; i < size_blocks; ++i) {
            size_t ind_index = (i - TAR_DIRECT_BLOCKS) / 64;
            size_t ind_offset = (i - TAR_DIRECT_BLOCKS) % 64;

            if (!hdr->indirect_blocks[ind_index]) {
                hdr->indirect_blocks[ind_index] = (uintptr_t) kmalloc(512);
                _assert(hdr->indirect_blocks[ind_index]);
                memset((void *) hdr->indirect_blocks[ind_index], 0, 512);
            }

            ((uintptr_t *) hdr->indirect_blocks[ind_index])[ind_offset] =
                (uintptr_t) block + 512 + i * 512 + 1;
        }

        if ((res = tarfs_mapper_create_path(tar_root, hdr->filename, &node, isdir)) < 0) {
            return res;
        }
        _assert(node);

        // Rewrite values in struct
        hdr->meta.uid = uid;
        hdr->meta.gid = gid;
        hdr->meta.mode = mode;
        hdr->meta.size = node_size;
        hdr->meta.mtime = mtime;
        hdr->meta.ctime = mtime;

        node->uid = hdr->meta.uid;
        node->gid = hdr->meta.gid;
        node->mode = hdr->meta.mode & VFS_MODE_MASK;

        node->fs_data = hdr;
        node->fs = tar;

        off += 512 + ((node_size + 511) & ~511);
    }

    return 0;
}

static struct vnode *tar_get_root(struct fs *tar) {
    return tar->fs_private;
}

static struct fs_class _tarfs = {
    .name = "ustar",
    .opt = 0,
    .init = tar_init,
    .get_root = tar_get_root
};

// Vnode operations

static int tarfs_vnode_stat(struct vnode *vn, struct stat *st) {
    _assert(st);
    _assert(vn);

    if (!vn->fs_data) {
        // Root node
        st->st_size = 0;
        st->st_blksize = 0;
        st->st_blocks = 0;

        st->st_ino = 0;

        st->st_uid = vn->uid;
        st->st_gid = vn->gid;
        st->st_mode = vn->mode | S_IFDIR;

        st->st_atime = system_boot_time;
        st->st_mtime = system_boot_time;
        st->st_ctime = system_boot_time;

        st->st_nlink = 1;
        st->st_rdev = 0;
        st->st_dev = 0;

        return 0;
    } else {
        struct tar *hdr = vn->fs_data;

        st->st_size = hdr->meta.size;
        st->st_blksize = 512;
        st->st_blocks = (st->st_size + 511) / 512;

        st->st_uid = vn->uid;
        st->st_gid = vn->gid;
        st->st_mode = (vn->mode & VFS_MODE_MASK) | (vn->type == VN_DIR ? S_IFDIR : S_IFREG);

        st->st_ino = 0;

        st->st_rdev = 0;
        st->st_dev = 0;

        st->st_mtime = hdr->meta.mtime;
        st->st_ctime = hdr->meta.ctime;
        st->st_atime = st->st_mtime;

        st->st_nlink = 1;

        return 0;
    }
}

static int tarfs_vnode_mkdir(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode) {
    _assert(at && at->type == VN_DIR);
    _assert(name);

    // Just create a vnode as we're only operating in memory
    struct vnode *node = vnode_create(VN_DIR, name);
    struct tar *hdr = kmalloc(512);
    _assert(hdr);

    hdr->meta.uid = uid;
    hdr->meta.gid = gid;
    hdr->meta.mode = mode & VFS_MODE_MASK;
    hdr->meta.size = 0;
    time_t cur_time = time();
    hdr->meta.mtime = cur_time;
    hdr->meta.ctime = cur_time;

    node->uid = uid;
    node->gid = gid;
    node->mode = mode & VFS_MODE_MASK;
    node->flags = VN_MEMORY;

    node->fs_data = hdr;
    node->fs = at->fs;
    node->op = at->op;

    vnode_attach(at, node);

    return 0;
}

static int tarfs_vnode_creat(struct vnode *at, const char *name, uid_t uid, gid_t gid, mode_t mode) {
    _assert(at && at->type == VN_DIR);
    _assert(name);

    // Just create a vnode as we're only operating in memory
    struct vnode *node = vnode_create(VN_REG, name);
    struct tar *hdr = kmalloc(512);
    _assert(hdr);

    hdr->meta.uid = uid;
    hdr->meta.gid = gid;
    hdr->meta.mode = mode & VFS_MODE_MASK;
    hdr->meta.size = 0;
    time_t cur_time = time();
    hdr->meta.mtime = cur_time;
    hdr->meta.ctime = cur_time;

    for (size_t i = 0; i < TAR_DIRECT_BLOCKS; ++i) {
        hdr->direct_blocks[i] = 0;
    }
    for (size_t i = 0; i < TAR_INDIRECT_BLOCKS; ++i) {
        hdr->indirect_blocks[i] = 0;
    }

    node->uid = uid;
    node->gid = gid;
    node->mode = mode & VFS_MODE_MASK;
    node->flags = VN_MEMORY;

    node->fs_data = hdr;
    node->fs = at->fs;
    node->op = at->op;

    vnode_attach(at, node);

    return 0;
}

static uintptr_t tarfs_block_addr(struct tar *hdr, uint32_t index) {
    if (index < TAR_DIRECT_BLOCKS) {
        return hdr->direct_blocks[index] & ~1;
    } else if ((index - TAR_DIRECT_BLOCKS) < (TAR_INDIRECT_BLOCKS * 64)) {
        uintptr_t *indirect = (uintptr_t *) hdr->indirect_blocks[(index - TAR_DIRECT_BLOCKS) / 64];
        _assert(indirect);
        return indirect[(index - TAR_DIRECT_BLOCKS) % 64] & ~1;
    } else {
        panic("Index is too high: %u\n", index);
    }
}

static void tarfs_block_free(struct tar *hdr, uint32_t index) {
    if (index < TAR_DIRECT_BLOCKS) {
        uintptr_t ptr = hdr->direct_blocks[index];

        if (!(ptr & 1)) {
            kfree((void *) ptr);
        }

        hdr->direct_blocks[index] = 0;
    } else {
        panic("TODO\n");
    }
}

static ssize_t tarfs_vnode_write(struct ofile *fd, const void *buf, size_t count) {
    _assert(fd);
    _assert(buf);
    struct vnode *node = fd->vnode;
    _assert(node);
    struct tar *hdr = node->fs_data;
    _assert(hdr);
    // Update mtime on writes
    hdr->meta.mtime = time();

    if (fd->pos > hdr->meta.size) {
        return -1;
    }

    size_t write_total = 0;
    size_t bwrite;

    while (count) {
        size_t blk_off = fd->pos % 512;
        size_t blk_ind = fd->pos / 512;
        bwrite = MIN(512 - blk_off, count);

        uintptr_t block_ptr = tarfs_block_addr(hdr, blk_ind);

        // This would mean non-zero offset in some unallocated block
        _assert(!blk_off || block_ptr);

        if (!block_ptr) {
            block_ptr = (uintptr_t) kmalloc(512);
            _assert(block_ptr);

            // Write block to block index
            if (blk_ind < TAR_DIRECT_BLOCKS) {
                hdr->direct_blocks[blk_ind] = block_ptr;
            } else {
                panic("TODO\n");
            }
        }

        // Copy data to destination
        memcpy((void *) (block_ptr + blk_off), buf, bwrite);

        fd->pos += bwrite;
        if (fd->pos > hdr->meta.size) {
            hdr->meta.size = fd->pos;
        }
        count -= bwrite;
        write_total += bwrite;
    }

    return write_total;
}

static ssize_t tarfs_vnode_read(struct ofile *fd, void *buf, size_t count) {
    _assert(fd);
    _assert(buf);
    struct vnode *node = fd->vnode;
    _assert(node);
    struct tar *hdr = node->fs_data;
    _assert(hdr);

    if (fd->pos >= hdr->meta.size) {
        return -1;
    }

    size_t can_read = MIN(count, hdr->meta.size - fd->pos);
    int res;
    size_t read_total = 0;
    uintptr_t blk_addr;

    while (can_read) {
        size_t blk_off = fd->pos % 512;
        size_t blk_ind = fd->pos / 512;
        size_t bread = MIN(can_read, 512 - blk_off);

        if ((blk_addr = tarfs_block_addr(hdr, blk_ind)) == MM_NADDR) {
            return -EIO;
        }

        memcpy(buf, (const void *) (blk_addr + blk_off), bread);

        can_read -= bread;
        buf += bread;
        fd->pos += bread;
        read_total += bread;
    }

    return read_total;
}

static int tarfs_vnode_open(struct ofile *of, int opt) {
    of->pos = 0;
    return 0;
}

static off_t tarfs_vnode_lseek(struct ofile *fd, off_t offset, int whence) {
    _assert(fd);
    struct vnode *node = fd->vnode;
    _assert(node);
    struct tar *hdr = node->fs_data;
    _assert(hdr);

    if (whence != SEEK_SET) {
        panic("NYI\n");
    }

    if (offset > hdr->meta.size) {
        return -EINVAL;
    }

    fd->pos = offset;
    kdebug("Seek to %p\n", fd->pos);

    return fd->pos;
}

static int tarfs_vnode_chmod(struct vnode *node, mode_t mode) {
    panic("NYI\n");
    return 0;
}

static int tarfs_vnode_chown(struct vnode *node, uid_t uid, gid_t gid) {
    panic("NYI\n");
    return 0;
}

static int tarfs_vnode_truncate(struct vnode *node, size_t length) {
    _assert(node);
    struct tar *hdr = node->fs_data;
    _assert(hdr);
    _assert(length == 0); // TODO: non-zero truncate for this

    size_t size_blocks = (hdr->meta.size + 511) / 512;

    for (ssize_t i = size_blocks - 1; i >= 0; --i) {
        size_t block_off = hdr->meta.size % 512;

        // Release block index
        tarfs_block_free(hdr, i);

        hdr->meta.size -= 512 - block_off;
    }

    return 0;
}

//

void tarfs_init(void) {
    fs_class_register(&_tarfs);
}
