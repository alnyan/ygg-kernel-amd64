#include "sys/fs/vfs.h"
#include "sys/fs/fs.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/fcntl.h"
#include "sys/chr.h"

static int vfs_find_internal(struct vfs_ioctx *ctx, struct vnode *at, const char *path, struct vnode **child);

static struct vnode *vfs_root = NULL;
static struct vfs_ioctx _kernel_ioctx = {0};

struct vfs_ioctx *const kernel_ioctx = &_kernel_ioctx;

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

static int vfs_link_resolve_internal(struct vfs_ioctx *ctx, struct vnode *lnk, struct vnode **to, int cnt) {
    _assert(lnk);

    if (lnk->type != VN_LNK) {
        *to = lnk;
        return 0;
    }

    if (!lnk->target && !(lnk->flags & VN_MEMORY)) {
        // Try to load link contents from storage
        _assert(lnk->op);
        _assert(lnk->op->readlink);
        _assert(lnk->parent);

        char path[PATH_MAX];
        int res;

        if ((res = lnk->op->readlink(lnk, path, PATH_MAX)) != 0) {
            return res;
        }

        // Try to load that path
        if ((res = vfs_find(ctx, lnk->parent, path, &lnk->target)) != 0) {
            lnk->target = NULL;
            return res;
        }
    }

    if (lnk->target) {
        if (lnk->target->type == VN_LNK) {
            if (cnt <= 1) {
                return -ELOOP;
            }

            return vfs_link_resolve_internal(ctx, lnk->target, to, cnt - 1);
        } else {
            *to = lnk->target;
            return 0;
        }
    }

    return -ENOENT;
}

int vfs_link_resolve(struct vfs_ioctx *ctx, struct vnode *lnk, struct vnode **to) {
    return vfs_link_resolve_internal(ctx, lnk, to, LINK_MAX);
}

static int vfs_lookup_or_load(struct vnode *at, const char *name, struct vnode **child) {
    int res;

    if ((res = vnode_lookup_child(at, name, child)) == 0) {
        return 0;
    }

    if (res != -ENOENT) {
        return res;
    }

    // Try to load vnode if possible
    if (!(at->flags & VN_MEMORY)) {
        _assert(at->op);
        _assert(at->op->find);
        struct vnode *node;

        if ((res = at->op->find(at, name, &node)) != 0) {
            return res;
        }

        // Attach found node to its parent
        vnode_attach(at, node);
        *child = node;

        return 0;
    }

    return -ENOENT;
}

static int vfs_find_internal(struct vfs_ioctx *ctx, struct vnode *at, const char *path, struct vnode **child) {
    // FIXME: trailing slash should result in symlink resolution in cases like:
    //      /dir
    //      /lnk -> /dir
    // And path is /lnk/ should result in /dir
    char name[NODE_MAXLEN];
    const char *child_path;
    struct vnode *node;
    int err;

    if (!at) {
        return -ENOENT;
    }

    _assert(child);

    if (!path || !*path) {
        // TODO: if mnt
        *child = at;
        kdebug("empty hit-final %s\n", at->name);
        return 0;
    }

    if (at->type == VN_LNK) {
        if ((err = vfs_link_resolve(ctx, at, &node)) != 0) {
            kdebug("miss: link resolution: %s\n", at->name);
            return err;
        } else {
            at = node;
        }
    }
    // TODO: links to links
    _assert(at->type != VN_LNK);

    if (at->type == VN_MNT) {
        at = at->target;
        _assert(at);
    }

    if (at->type != VN_DIR) {
        // Only directories contain files
        return -ENOTDIR;
    }

    // TODO: check access to "at" here

    // The paths are always assumed to be relative
    while (*path == '/') {
        ++path;
    }

    while (1) {
        child_path = path_element(path, name);

        if (!strcmp(name, ".")) {
            // Refers to this node
            kdebug(". hit %s\n", at->name);
            if (!child_path) {
                *child = at;
                return 0;
            }
            continue;
        } else if (!strcmp(name, "..")) {
            // Refers to node's parent
            // And this won't work for filesystem mountpoints:
            // Path like a/mnt/.. would still resolve the same point as
            // a/mnt
            struct vnode *parent = at->parent;

            if (!parent) {
                parent = at;
            }
            kdebug(".. hit %s\n", parent->name);
            return vfs_find_internal(ctx, parent, child_path, child);
        }

        break;
    }

    // TODO: lookup_or_load
    if ((err = vfs_lookup_or_load(at, name, &node)) != 0) {
        kdebug("miss %s in %s\n", name, at->name);
        return err;
    }

    _assert(node);

    if (!child_path) {
        kdebug("hit-final %s\n", name);
        *child = node;
        return 0;
    } else {
        kdebug("hit %s\n", name);
        return vfs_find_internal(ctx, node, child_path, child);
    }

    return -ENOENT;
}

void vfs_init(void) {
    if (vfs_root) {
        vnode_dump_tree(DEBUG_INFO, vfs_root, 0);
    }
}

static int vfs_mount_internal(struct vnode *at, void *blk, struct fs_class *cls, const char *opt) {
    int res;
    struct fs *fs;
    struct vnode *fs_root;
    struct blkdev *dev = blk;

    // Device checks
    if (blk) {
        if (dev->flags & BLK_BUSY) {
            return -EBUSY;
        }
    }

    // Mountpoint checks
    if (at) {
        if (at->type != VN_DIR) {
            return -ENOTDIR;
        }
    }

    if (!(fs = fs_create(cls, blk))) {
        kwarn("Filesystem creation failed\n");
        return -EINVAL;
    }

    _assert(cls->get_root);

    if (!(fs_root = cls->get_root(fs))) {
        kwarn("Filesystem has no root\n");
        // TODO: cleanup
        return -EINVAL;
    }

    // Attach root to mountpoint
    if (!at) {
        // Make filesystem root new VFS root
        vfs_root = fs_root;
    } else {
        at->type = VN_MNT;
        at->target = fs_root;

        // TODO: this prevents root nodes from being mounted at
        // multiple targets
        fs_root->parent = at->parent;
    }

    if (blk) {
        dev->flags |= BLK_BUSY;
    }

    vnode_dump_tree(DEBUG_DEFAULT, vfs_root, 0);
    // TODO: call cls->mount

    return 0;
}

int vfs_mount(struct vfs_ioctx *ctx, const char *at, void *blk, const char *fs, const char *opt) {
    int res;
    struct fs_class *fs_class;
    struct vnode *mountpoint;

    // NOTE: Mount uses only absolute paths
    if (at[0] != '/') {
        kwarn("Non-absolute path used for mounting\n");
        return -EINVAL;
    }

    if (!fs || !strcmp(fs, "auto")) {
        panic("TODO: support filesystem guessing\n");
    } else {
        fs_class = fs_class_by_name(fs);

        if (!fs_class) {
            kwarn("Unknown fs: %s\n", fs);
            return -EINVAL;
        }
    }

    // Find mount point
    if (!strcmp(at, "/")) {
        mountpoint = NULL;
    } else {
        if ((res = vfs_find(ctx, ctx->cwd_vnode, at, &mountpoint)) != 0) {
            return res;
        }
    }

    return vfs_mount_internal(mountpoint, blk, fs_class, opt);
}

int vfs_find(struct vfs_ioctx *ctx, struct vnode *rel, const char *path, struct vnode **node) {
    int res;
    struct vnode *r;

    if (!vfs_root) {
        return -ENOENT;
    }

    if (path[0] == '/') {
        // Absolute lookup
        while (*path == '/') {
            ++path;
        }
        res = vfs_find_internal(ctx, vfs_root, path, &r);
    } else {
        if (!rel) {
            rel = ctx->cwd_vnode;
        }
        if (!rel) {
            rel = vfs_root;
        }
        res = vfs_find_internal(ctx, rel, path, &r);
    }

    if (res) {
        return res;
    }

    if (r->type == VN_MNT) {
        r = r->target;
    }

    *node = r;

    return 0;
}

///////////

int vfs_open_vnode(struct vfs_ioctx *ctx, struct ofile *fd, struct vnode *node, int opt) {
    int res;

    _assert(ctx);
    _assert(fd);
    _assert(node);

    if (opt & O_DIRECTORY) {
        panic("NYI: O_DIRECTORY\n");
    }

    // 1. If file operations struct specifies some non-trivial open()
    //    function for the node - use it
    if (node->op && node->op->open) {
        if ((res = node->op->open(node, opt)) != 0) {
            return res;
        }
    }

    // 2. Setup ofile
    fd->pos = 0;
    fd->vnode = node;
    fd->flags = 0;

    switch (opt & O_ACCMODE) {
    case O_RDONLY:
        fd->flags |= OF_READABLE;
        break;
    case O_WRONLY:
        fd->flags |= OF_WRITABLE;
        break;
    case O_RDWR:
        fd->flags |= OF_READABLE | OF_WRITABLE;
        break;
    }

    return 0;
}

int vfs_open(struct vfs_ioctx *ctx, struct ofile *fd, const char *path, int opt, int mode) {
    struct vnode *node;
    int res;

    _assert(ctx);
    _assert(fd);
    _assert(path);

    // 1. Try to find the file
    if ((res = vfs_find(ctx, ctx->cwd_vnode, path, &node)) != 0) {
        if (opt & O_CREAT) {
            panic("NYI: O_CREAT\n");
        } else {
            return res;
        }
    }

    return vfs_open_vnode(ctx, fd, node, opt);
}

void vfs_close(struct vfs_ioctx *ctx, struct ofile *fd) {
    // TODO: TODO
}

int vfs_stat(struct vfs_ioctx *ctx, const char *path, struct stat *st) {
    struct vnode *node;
    int res;

    _assert(ctx);
    _assert(path);
    _assert(st);

    if ((res = vfs_find(ctx, ctx->cwd_vnode, path, &node)) != 0) {
        return res;
    }

    if (!node->op || !node->op->stat) {
        kwarn("%s: filesystem has no stat()\n", path);
        return -EINVAL;
    }

    return node->op->stat(node, st);
}

ssize_t vfs_write(struct vfs_ioctx *ctx, struct ofile *fd, const void *buf, size_t count) {
    struct vnode *node;

    _assert(fd);

    if (!(fd->flags & OF_WRITABLE) || (fd->flags & OF_DIRECTORY)) {
        return -EINVAL;
    }

    node = fd->vnode;

    _assert(ctx);
    _assert(buf);
    _assert(node);

    switch (node->type) {
    case VN_REG:
        _assert(node->op && node->op->write);
        return node->op->write(fd, buf, count);
    case VN_CHR:
        _assert(node->dev && ((struct chrdev *) node->dev)->write);
        return chr_write(node->dev, buf, fd->pos, count);

    case VN_DIR:
    default:
        // Cannot do such things
        return -EINVAL;
    }
}

ssize_t vfs_read(struct vfs_ioctx *ctx, struct ofile *fd, void *buf, size_t count) {
    struct vnode *node;

    if (!(fd->flags & OF_READABLE)) {
        return -EINVAL;
    }

    _assert(fd);

    node = fd->vnode;

    _assert(ctx);
    _assert(buf);
    _assert(node);

    switch (node->type) {
    case VN_REG:
        _assert(node->op && node->op->read);
        return node->op->read(fd, buf, count);
    case VN_CHR:
        _assert(node->dev && ((struct chrdev *) node->dev)->read);
        return chr_read(node->dev, buf, fd->pos, count);

    case VN_DIR:
    default:
        // Cannot do such things
        return -EINVAL;
    }
}
