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
    size_t first_block;
    size_t size;
};

static ssize_t tarfs_vnode_read(struct ofile *fd, void *buf, size_t count);

static struct vnode_operations _tarfs_vnode_op = {
    .read = tarfs_vnode_read,
    NULL
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
    char block[512];
    size_t off = 0;
    int prev_zero = 0;
    ssize_t res;

    // Create filesystem root node
    struct vnode *tar_root = vnode_create(VN_DIR, NULL);
    tar->fs_private = tar_root;
    tar_root->fs = tar;
    tar_root->flags |= VN_MEMORY;

    struct vnode *node;

    while (1) {
        if ((res = blk_read(tar->blk, block, off, 512)) < 0) {
            return res;
        }

        struct tar *hdr = (struct tar *) block;

        if (!hdr->filename[0]) {
            if (prev_zero) {
                break;
            }

            prev_zero = 1;
            off += 512;
            continue;
        }

        size_t node_size = 0;
        int isdir = 1;

        if (hdr->typeflag[0] == '\0' || hdr->typeflag[0] == '0') {
            node_size = tarfs_octal(hdr->size, 12);
            isdir = 0;
        }

        if ((res = tarfs_mapper_create_path(tar_root, hdr->filename, &node, isdir)) < 0) {
            return res;
        }
        _assert(node);

        // Initialize the vnode
        node->uid = 0;
        node->gid = 0;

        struct tarfs_vnode_attr *attr = kmalloc(sizeof(struct tarfs_vnode_attr));
        _assert(attr);
        node->fs_data = attr;
        node->fs = tar;

        attr->first_block = off + 512;
        attr->size = node_size;

        off += 512 + ((node_size + 0x1FF) & ~0x1FF);
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

static ssize_t tarfs_vnode_read(struct ofile *fd, void *buf, size_t count) {
    _assert(fd);
    _assert(buf);
    _assert(fd->vnode);
    struct vnode *vn = fd->vnode;
    _assert(vn->fs_data);
    _assert(vn->fs && vn->fs->blk);
    struct tarfs_vnode_attr *attr = vn->fs_data;

    if (fd->pos >= attr->size) {
        return -1;
    }

    size_t can = MIN(attr->size - fd->pos, count);
    // TODO: size is not block-size aligned, may fuck up
    blk_read(vn->fs->blk, buf, fd->pos + attr->first_block, can);

    return can;
}

//

void tarfs_init(void) {
    fs_class_register(&_tarfs);
}
