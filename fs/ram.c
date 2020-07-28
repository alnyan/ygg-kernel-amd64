#include "sys/block/blk.h"
#include "sys/block/ram.h"
#include "sys/mem/slab.h"
#include "sys/mem/phys.h"
#include "fs/ram_tar.h"
#include "user/fcntl.h"
#include "user/errno.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/attr.h"
#include "fs/node.h"
#include "fs/ram.h"
#include "sys/mm.h"
#include "fs/vfs.h"
#include "fs/fs.h"

// Forward declaration for RAM vnode manip
static struct vnode_operations _ramfs_vnode_op;

// Each Ln (n >= 1) block contains 512 L(n-1) block pointers
// L0 block pointer is a direct pointer to file data
// So L0 -> 512 bytes
// Ln (n > 0) -> 4K bytes
#define RAM_BLOCKS_L0           32
#define RAM_BLOCKS_L1           24
#define RAM_BLOCKS_PER_Ln       512

// Maximum achievable blocks at L1
#define RAM_MAX_L1              (RAM_BLOCKS_L1 * RAM_BLOCKS_PER_Ln)

// BSet block index flags
#define RAM_BSET_AVAIL          (1 << 0)        // Means the index is available for use (see resize)
#define RAM_BSET_ALLOC          (1 << 1)        // Means the target block was kmalloc'd

// WARN: This struct should fit in 512 bytes, as it's
//       a limit for the slab allocator as of yet
struct ram_vnode_private {
    size_t size;
    // Block pointer array
    size_t bpa_cap;
    uintptr_t bpa_l0[RAM_BLOCKS_L0];
    uintptr_t *bpa_l1[RAM_BLOCKS_L1];
    uintptr_t **bpa_l2;
};

// RAM vnode manip

static struct slab_cache *ram_vnode_private_cache;

struct vnode *ram_vnode_create(enum vnode_type t, const char *filename) {
    struct vnode *vn;
    struct ram_vnode_private *priv;

    if (!ram_vnode_private_cache) {
        ram_vnode_private_cache = slab_cache_get(sizeof(struct ram_vnode_private));
        _assert(ram_vnode_private_cache);
    }

    vn = vnode_create(t, filename);
    if (!vn) {
        return NULL;
    }

    // No need to setup, everything is zeroed by calloc
    priv = slab_calloc(ram_vnode_private_cache);
    _assert(priv);

    vn->fs = NULL;
    vn->op = &_ramfs_vnode_op;
    vn->fs_data = priv;
    vn->flags |= VN_MEMORY;

    return vn;
}

int ram_vnode_bset_resize(struct vnode *vn, size_t size) {
    struct ram_vnode_private *priv;
    uintptr_t blk;
    uintptr_t *index_dst;
    size_t l1_index;
    size_t l0_index;
    size_t count;

    priv = vn->fs_data;

    count = (size + 511) / 512;

    if (priv->bpa_cap > count) {
        // Free the blocks
        for (size_t i = count; i < priv->bpa_cap; ++i) {
            if (i < RAM_BLOCKS_L0) {
                blk = priv->bpa_l0[i];
                priv->bpa_l0[i] = 0;
            } else if (i < (RAM_BLOCKS_L0 + RAM_BLOCKS_L1 * RAM_BLOCKS_PER_Ln)) {
                l1_index = (i - RAM_BLOCKS_L0) / RAM_BLOCKS_PER_Ln;
                l0_index = (i - RAM_BLOCKS_L0) % RAM_BLOCKS_PER_Ln;
                _assert(priv->bpa_l1[l1_index]);
                blk = priv->bpa_l1[l1_index][l0_index];
            } else {
                panic("Block index too large: %u\n", i);
            }

            _assert(blk & RAM_BSET_AVAIL);
            if (blk & RAM_BSET_ALLOC) {
                blk &= ~511;
                mm_phys_free_page(MM_PHYS(blk));
            }
        }

        // Free L1 structures
        if (priv->bpa_cap > RAM_BLOCKS_L0 && count <= (RAM_BLOCKS_L0 + RAM_BLOCKS_L1 * RAM_BLOCKS_PER_Ln)) {
            // L1 block count
            size_t block_index = MIN((priv->bpa_cap - RAM_BLOCKS_L0 + RAM_BLOCKS_PER_Ln - 1) / RAM_BLOCKS_PER_Ln, RAM_BLOCKS_L1);
            // New L1 count
            size_t block_start = (count + RAM_BLOCKS_PER_Ln - RAM_BLOCKS_L0 - 1) / RAM_BLOCKS_PER_Ln;
            _assert(block_start <= block_index);

            for (size_t i = block_start; i < block_index; ++i) {
                _assert(priv->bpa_l1[l1_index]);
                mm_phys_free_page(MM_PHYS(priv->bpa_l1[l1_index]));
                priv->bpa_l1[l1_index] = NULL;
            }
        }
        // TODO: handle L2
    } else {
        for (size_t i = priv->bpa_cap; i < count; ++i) {
            if (i < RAM_BLOCKS_L0) {
                // Make sure no block is written beyound capacity
                _assert(!priv->bpa_l0[i]);
                index_dst = &priv->bpa_l0[i];
            } else if (i < (RAM_BLOCKS_L0 + RAM_BLOCKS_L1 * RAM_BLOCKS_PER_Ln)) {
                l1_index = (i - RAM_BLOCKS_L0) / RAM_BLOCKS_PER_Ln;
                l0_index = (i - RAM_BLOCKS_L0) % RAM_BLOCKS_PER_Ln;

                // Allocate L1 block, if it hasn't been yet
                if (!priv->bpa_l1[l1_index]) {
                    blk = mm_phys_alloc_page();
                    _assert(blk != MM_NADDR);
                    priv->bpa_l1[l1_index] = (uintptr_t *) MM_VIRTUALIZE(blk);
                    memset(priv->bpa_l1[l1_index], 0, RAM_BLOCKS_PER_Ln * sizeof(uintptr_t));
                }

                index_dst = &(priv->bpa_l1[l1_index][l0_index]);
            } else {
                panic("Block index too large: %u\n", i);
            }

            // Mark the index as "available for use"
            *index_dst = RAM_BSET_AVAIL;
        }
    }

    priv->size = size;
    priv->bpa_cap = count;

    return 0;
}

uintptr_t ram_vnode_bset_get(struct vnode *vn, size_t index) {
    uintptr_t *index_src;
    struct ram_vnode_private *priv;
    size_t l1_index, l0_index;

    priv = vn->fs_data;

    // TODO: this code is cloned three times, try to eliminate this later
    if (index < RAM_BLOCKS_L0) {
        index_src = &priv->bpa_l0[index];
    } else if (index < (RAM_BLOCKS_L0 + RAM_BLOCKS_L1 * RAM_BLOCKS_PER_Ln)) {
        l1_index = (index - RAM_BLOCKS_L0) / RAM_BLOCKS_PER_Ln;
        l0_index = (index - RAM_BLOCKS_L0) % RAM_BLOCKS_PER_Ln;

        _assert(priv->bpa_l1[l1_index]);
        index_src = &(priv->bpa_l1[l1_index][l0_index]);
    } else {
        panic("Block index too large: %u\n", index);
    }

    _assert((*index_src) & RAM_BSET_AVAIL);
    return *index_src;
}

int ram_vnode_bset_set(struct vnode *vn, size_t index, uintptr_t block) {
    uintptr_t *index_dst;
    struct ram_vnode_private *priv;
    size_t l1_index, l0_index;

    priv = vn->fs_data;

    // TODO: allow calls with NULL block to automatically allocate one
    // Blocks must be aligned
    _assert(!(block & 511) || (block & RAM_BSET_ALLOC));
    // This doesn't mean the index can be reused, just that the index is
    // within capacity limit and is legit
    block |= RAM_BSET_AVAIL;

    if (index < RAM_BLOCKS_L0) {
        index_dst = &priv->bpa_l0[index];
    } else if (index < (RAM_BLOCKS_L0 + RAM_BLOCKS_L1 * RAM_BLOCKS_PER_Ln)) {
        l1_index = (index - RAM_BLOCKS_L0) / RAM_BLOCKS_PER_Ln;
        l0_index = (index - RAM_BLOCKS_L0) % RAM_BLOCKS_PER_Ln;

        _assert(priv->bpa_l1[l1_index]);
        index_dst = &(priv->bpa_l1[l1_index][l0_index]);
    } else {
        panic("Block index too large: %u\n", index);
    }

    // Validate that:
    // 1. No block has been placed here before
    // 2. It's a valid index within limits
    _assert(*index_dst == RAM_BSET_AVAIL);
    *index_dst = block;

    return 0;
}

// RAM fs stuff
static int ramfs_vnode_open(struct ofile *of, int opt);
static int ramfs_vnode_stat(struct vnode *vn, struct stat *st);
static ssize_t ramfs_vnode_read(struct ofile *of, void *buf, size_t count);
static ssize_t ramfs_vnode_write(struct ofile *of, const void *buf, size_t count);
static off_t ramfs_vnode_lseek(struct ofile *of, off_t off, int whence);
static int ramfs_vnode_creat(struct vnode *at, const char *filename, uid_t uid, gid_t gid, mode_t mode);
static int ramfs_vnode_truncate(struct vnode *at, size_t size);
static int ramfs_vnode_mkdir(struct vnode *at, const char *filename, uid_t uid, gid_t gid, mode_t mode);
static int ramfs_vnode_unlink(struct vnode *node);

static struct vnode_operations _ramfs_vnode_op = {
    .open = ramfs_vnode_open,
    .stat = ramfs_vnode_stat,
    .read = ramfs_vnode_read,
    .write = ramfs_vnode_write,
    .lseek = ramfs_vnode_lseek,
    .creat = ramfs_vnode_creat,
    .mkdir = ramfs_vnode_mkdir,
    .truncate = ramfs_vnode_truncate,
    .unlink = ramfs_vnode_unlink,
};

static int ram_init(struct fs *ramfs, const char *opt) {
    struct vnode *ramfs_root;
    void *mem_base;
    ssize_t res;

    // Only supported format (as of yet)
    // is in-memory TAR
    if ((res = blk_ioctl(ramfs->blk, RAM_IOC_GETBASE, &mem_base)) != 0) {
        return res;
    }

    // Create filesystem root node
    ramfs_root = vnode_create(VN_DIR, NULL);
    ramfs_root->fs = ramfs;
    ramfs_root->flags |= VN_MEMORY;
    ramfs_root->op = &_ramfs_vnode_op;
    ramfs_root->fs_data = NULL;

    ramfs_root->uid = 0;
    ramfs_root->gid = 0;
    ramfs_root->mode = 0555;

    ramfs->fs_private = ramfs_root;

    return tar_init(ramfs, mem_base);
}

static struct vnode *ram_get_root(struct fs *ram) {
    return ram->fs_private;
}

static struct fs_class _ramfs = {
    .name = "ramfs",
    .opt = 0,
    .init = ram_init,
    .get_root = ram_get_root
};

// vnode function implementation

static int ramfs_vnode_open(struct ofile *of, int opt) {
    if (of->flags & OF_WRITABLE) {
        // TODO: check if vnode is locked for write
    }
    return 0;
}

static int ramfs_vnode_stat(struct vnode *vn, struct stat *st) {
    struct ram_vnode_private *priv;
    _assert(vn->fs_data);

    priv = vn->fs_data;

    memset(st, 0, sizeof(struct stat));
    // TODO: appropriate masks
    st->st_mode = vn->mode;
    switch (vn->type) {
    case VN_REG:
        st->st_mode |= S_IFREG;
        break;
    case VN_LNK:
        st->st_mode |= S_IFLNK;
        break;
    case VN_DIR:
        st->st_mode |= S_IFDIR;
        break;
    default:
        panic("TODO\n");
    }
    st->st_uid = vn->uid;
    st->st_gid = vn->gid;
    if (vn->type == VN_REG) {
        st->st_size = priv->size;
        st->st_blocks = priv->bpa_cap;
    } else {
        st->st_size = 512;  // TODO: correct size for symlinks
        st->st_blocks = 1;
    }
    st->st_blksize = 512;

    return 0;
}

static ssize_t ramfs_vnode_read(struct ofile *of, void *buf, size_t count) {
    struct ram_vnode_private *priv;
    uintptr_t blk;
    size_t block_offset, block_index;
    size_t off, can_read;
    size_t rem;
    struct vnode *vn;

    vn = of->file.vnode;
    _assert(vn->fs_data);

    priv = vn->fs_data;

    if (of->file.pos >= priv->size) {
        of->file.pos = priv->size;
        return 0;
    }

    rem = MIN(count, priv->size - of->file.pos);
    off = 0;
    while (rem) {
        block_index = of->file.pos / 512;
        block_offset = of->file.pos % 512;
        can_read = MIN(rem, 512 - block_offset);
        _assert(can_read);

        blk = ram_vnode_bset_get(vn, block_index);
        _assert(blk & ~511);

        // Strip flags from block ptr
        blk &= ~511;

        memcpy(buf + off, (const void *) blk + block_offset, can_read);

        off += can_read;
        rem -= can_read;
        of->file.pos += can_read;
    }

    return off;
}

static ssize_t ramfs_vnode_write(struct ofile *of, const void *buf, size_t count) {
    struct ram_vnode_private *priv;
    struct vnode *vn;
    uintptr_t blk;
    size_t block_offset, block_index;
    size_t off, can_write;
    size_t rem;

    _assert(vn = of->file.vnode);
    _assert(priv = vn->fs_data);

    if (of->file.pos > priv->size) {
        panic("This shouldn't happen\n");
    }

    // First grow the file index array
    if (of->file.pos + count > priv->size) {
        // Number of last block
        block_index = (of->file.pos + count + 511) / 512;
        // Old capacity
        block_offset = priv->bpa_cap;

        _assert(ram_vnode_bset_resize(vn, of->file.pos + count) == 0);

        for (size_t i = block_offset; i < block_index; ++i) {
            // TODO: this is suboptimal - 3.5K get wasted
            blk = mm_phys_alloc_page();
            _assert(blk != MM_NADDR);
            blk = MM_VIRTUALIZE(blk) | RAM_BSET_ALLOC;
            _assert(ram_vnode_bset_set(vn, i, blk) == 0);
        }
    }

    rem = count;
    off = 0;
    while (rem) {
        block_index = of->file.pos / 512;
        block_offset = of->file.pos % 512;
        can_write = MIN(rem, 512 - block_offset);
        _assert(can_write);

        blk = ram_vnode_bset_get(vn, block_index);
        _assert(blk & ~511);

        blk &= ~511;

        memcpy((void *) blk + block_offset, buf + off, can_write);

        off += can_write;
        rem -= can_write;
        of->file.pos += can_write;
    }

    return off;
}

static off_t ramfs_vnode_lseek(struct ofile *of, off_t off, int whence) {
    struct ram_vnode_private *priv;
    struct vnode *vn;

    vn = of->file.vnode;
    _assert(vn->fs_data);

    priv = vn->fs_data;

    switch (whence) {
    case SEEK_SET:
        if ((size_t) off > priv->size) {
            return (off_t) -ESPIPE;
        }
        of->file.pos = off;
        break;
    case SEEK_CUR:
        if ((off + (ssize_t) of->file.pos) < 0 || (size_t) (off + of->file.pos) > priv->size) {
            return (off_t) -ESPIPE;
        }
        of->file.pos += off;
        break;
    }

    return of->file.pos;
}

static int ramfs_vnode_creat(struct vnode *at, const char *filename, uid_t uid, gid_t gid, mode_t mode) {
    struct vnode *vn = ram_vnode_create(VN_REG, filename);
    if (!vn) {
        return -ENOMEM;
    }

    vn->uid = uid;
    vn->gid = gid;
    vn->mode = mode & VFS_MODE_MASK;

    vnode_attach(at, vn);

    return 0;
}

static int ramfs_vnode_mkdir(struct vnode *at, const char *filename, uid_t uid, gid_t gid, mode_t mode) {
    struct vnode *vn = ram_vnode_create(VN_DIR, filename);
    if (!vn) {
        return -ENOMEM;
    }

    vn->uid = uid;
    vn->gid = gid;
    vn->mode = mode & VFS_MODE_MASK;

    vnode_attach(at, vn);

    return 0;
}

static int ramfs_vnode_unlink(struct vnode *node) {
    int res;

    if (node->type == VN_DIR && node->first_child) {
        return -EEXIST;
    }

    if (node->type == VN_REG) {
        // Free the blocks by truncating the file to zero
        res = ramfs_vnode_truncate(node, 0);
        if (res != 0) {
            return res;
        }
    }

    slab_free(ram_vnode_private_cache, node->fs_data);
    node->fs_data = NULL;

    // VFS will do its job
    return 0;
}

static int ramfs_vnode_truncate(struct vnode *at, size_t size) {
    struct ram_vnode_private *priv;
    _assert(priv = at->fs_data);
    // TODO: allocate blocks on upward truncation
    if (size > priv->size) {
        panic("NYI\n");
    }
    _assert(ram_vnode_bset_resize(at, size) == 0);

    return 0;
}

__init(ramfs_init) {
    fs_class_register(&_ramfs);
}
