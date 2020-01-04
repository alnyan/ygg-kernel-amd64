#include "sys/fs/tar.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "sys/heap.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/errno.h"
#include "sys/fs/vfs.h"
#include "sys/fcntl.h"

struct tar {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag[1];
};

struct tarfs_vnode_attr {
    uint32_t type_perm;
    uint32_t uid;
    uint32_t gid;
    size_t block;
    size_t size;
};

static int tarfs_vnode_access(vnode_t *vn, uid_t *uid, gid_t *gid, mode_t *mode);
static int tarfs_vnode_stat(vnode_t *vn, struct stat *st);
static int tarfs_vnode_open(vnode_t *vn, int opt);
static ssize_t tarfs_vnode_read(struct ofile *fd, void *buf, size_t count);

static struct vnode_operations _tarfs_vnode_op = {
    .access = tarfs_vnode_access,
    .stat = tarfs_vnode_stat,
    .open = tarfs_vnode_open,
    .read = tarfs_vnode_read
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

static const char *tarfs_path_element(char *dst, const char *src) {
    const char *sep = strchr(src, '/');
    if (!sep) {
        strcpy(dst, src);
        return NULL;
    } else {
        strncpy(dst, src, sep - src);
        dst[sep - src] = 0;
        while (*sep == '/') {
            ++sep;
        }
        if (!*sep) {
            return NULL;
        }
        return sep;
    }
}

static int tarfs_node_add(struct vfs_node *at, struct vfs_node **res, const char *name) {
    // Try to find the node
    for (struct vfs_node *child = at->child; child; child = child->cdr) {
        if (!strcmp(child->name, name)) {
            *res = child;
            // Already exists
            return 0;
        }
    }

    // Else we need to insert it
    struct vfs_node *node_new = (struct vfs_node *) kmalloc(sizeof(struct vfs_node));
    strcpy(node_new->name, name);
    node_new->ismount = 0;
    node_new->parent = at;
    node_new->child = NULL;
    node_new->cdr = at->child;
    node_new->real_vnode = NULL;
    node_new->vnode = NULL;
    at->child = node_new;
    *res = node_new;

    return 0;
}

static int tarfs_mapper_create_path(struct vfs_node *root, struct vfs_node **res, const char *path, int isdir) {
    char path_element[256];
    const char *remaining = path;
    struct vfs_node *node = root;
    struct vfs_node *new_node;

    kdebug("Add path %s\n", path);
    while (1) {
        remaining = tarfs_path_element(path_element, remaining);

        if (tarfs_node_add(node, &new_node, path_element) < 0) {
            panic("Failed\n");
        }

        node = new_node;

        if (!remaining) {
            break;
        }
    }

    *res = node;

    return 0;
}

static vnode_t *tarfs_create_vnode(fs_t *fs, struct vfs_node *node, struct tar *hdr, size_t off) {
    if (hdr) {
        struct tarfs_vnode_attr *attr = (struct tarfs_vnode_attr *) kmalloc(sizeof(struct tarfs_vnode_attr));
        attr->block = off + 512;
        attr->gid = tarfs_octal(hdr->gid, 8);
        attr->uid = tarfs_octal(hdr->uid, 8);
        attr->type_perm = tarfs_octal(hdr->mode, 8);

        if (!(hdr->typeflag[0] == '\0' || hdr->typeflag[0] == '0')) {
            attr->type_perm |= (1 << 16);
            attr->size = 0;
        } else {
            attr->size = tarfs_octal(hdr->size, 12);
        }

        vnode_t *res = (vnode_t *) kmalloc(sizeof(vnode_t));
        res->tree_node = node;
        res->fs = fs;
        res->fs_data = attr;
        res->refcount = 0;
        res->op = &_tarfs_vnode_op;
        res->type = (attr->type_perm & (1 << 16)) ? VN_DIR : VN_REG;

        return res;
    } else {

        vnode_t *res = (vnode_t *) kmalloc(sizeof(vnode_t));
        res->tree_node = node;
        res->fs = fs;
        res->fs_data = NULL;
        res->refcount = 0;
        res->op = &_tarfs_vnode_op;
        res->type = VN_DIR;

        return res;
    }
}

static int tarfs_node_mapper(fs_t *tar, struct vfs_node **root) {
    kdebug("tar: node mapper\n");
    char block_buffer[512];
    size_t off = 0;
    int was0 = 0;

    struct vfs_node *_root = (struct vfs_node *) kmalloc(sizeof(struct vfs_node));
    _root->child = NULL;
    _root->cdr = NULL;
    _root->ismount = 0;
    _root->parent = NULL;
    _root->real_vnode = NULL;
    _root->vnode = NULL;
    struct vfs_node *curr_node;

    while (1) {
        if (blk_read(tar->blk, block_buffer, off, 512) < 0) {
            return -EIO;
        }
        struct tar *hdr = (struct tar *) block_buffer;

        if (hdr->filename[0] == 0) {
            if (was0) {
                // End of archive
                break;
            } else {
                was0 = 1;
                off += 512;
                continue;
            }
        }

        size_t node_size = 0;
        int isdir = 1;

        if (hdr->typeflag[0] == '\0' || hdr->typeflag[0] == '0') {
            node_size = tarfs_octal(hdr->size, 12);
            isdir = 0;
        }

        kdebug("Node %c:%s:%S\n", isdir ? 'd' : '-', hdr->filename, node_size);

        tarfs_mapper_create_path(_root, &curr_node, hdr->filename, isdir);
        _assert(curr_node);
        vnode_t *vnode = tarfs_create_vnode(tar, curr_node, hdr, off);
        _assert(vnode);
        curr_node->vnode = vnode;

        off += 512 + ((node_size + 0x1FF) & ~0x1FF);
    }

    // Make a vnode for root
    vnode_t *vnode = tarfs_create_vnode(tar, _root, NULL, 0);
    _root->vnode = vnode;

    *root = _root;

    return 0;
}

static struct fs_class _tarfs = {
    .name = "ustar",
    .opt = FS_NODE_MAPPER,
    .mapper = tarfs_node_mapper
};

// Vnode operations

static int tarfs_vnode_access(vnode_t *vn, uid_t *uid, gid_t *gid, mode_t *mode) {
    _assert(vn && uid && gid && mode);

    if (!vn->fs_data) {
        *uid = 0;
        *gid = 0;
        *mode = S_IFDIR | 0755;
    } else {
        struct tarfs_vnode_attr *attr = (struct tarfs_vnode_attr *) vn->fs_data;
        *uid = attr->uid;
        *gid = attr->gid;
        *mode = (attr->type_perm & 0x1FF) | ((attr->type_perm & (1 << 16)) ? S_IFDIR : S_IFREG);
    }

    return 0;
}

static int tarfs_vnode_stat(vnode_t *vn, struct stat *st) {
    _assert(vn && st);
    if (!vn->fs_data) {
        // Root node
        st->st_atime = 0;
        st->st_ctime = 0;
        st->st_mtime = 0;
        st->st_dev = 0;
        st->st_rdev = 0;

        st->st_gid = 0;
        st->st_uid = 0;
        st->st_mode = S_IFDIR | 0755;

        st->st_ino = 0;
        st->st_nlink = 1;

        st->st_blksize = 512;
        st->st_blocks = 0;
        st->st_size = 0;

        return 0;
    }
    struct tarfs_vnode_attr *attr = (struct tarfs_vnode_attr *) vn->fs_data;

    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    st->st_dev = 0;
    st->st_rdev = 0;

    st->st_gid = attr->gid;
    st->st_uid = attr->uid;
    st->st_mode = (attr->type_perm & 0x1FF) | ((attr->type_perm & (1 << 16)) ? S_IFDIR : S_IFREG);

    st->st_ino = (attr->block / 512) - 1;
    st->st_nlink = 1;

    st->st_blksize = 512;
    st->st_blocks = (attr->size + 511) & ~511;
    st->st_size = attr->size;

    return 0;
}

static int tarfs_vnode_open(vnode_t *vnode, int opt) {
    if ((opt & O_ACCMODE) != O_RDONLY) {
        return -EROFS;
    }
    return 0;
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))
static ssize_t tarfs_vnode_read(struct ofile *fd, void *buf, size_t count) {
    _assert(fd && buf && fd->vnode);
    vnode_t *vn = fd->vnode;
    _assert(vn->fs_data);
    struct tarfs_vnode_attr *attr = (struct tarfs_vnode_attr *) vn->fs_data;

    if (fd->pos >= attr->size) {
        return -1;
    }

    size_t can = MIN(attr->size - fd->pos, count);
    // TODO: do it properly with block buffer
    blk_read(vn->fs->blk, buf, fd->pos + attr->block, can);

    return can;
}

//

void tarfs_init(void) {
    fs_class_register(&_tarfs);
}
