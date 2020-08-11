#include "user/fcntl.h"
#include "user/errno.h"
#include "sys/thread.h"
#include "sys/block/blk.h"
#include "fs/node.h"
#include "sys/heap.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "fs/vfs.h"
#include "fs/fs.h"
#include "sys/panic.h"
#include "sys/debug.h"

static int vfs_find_internal(struct vfs_ioctx *ctx,
                             struct vnode *at,
                             const char *path,
                             int link_itself,
                             struct vnode **child);

static struct vnode *vfs_root = NULL;
static struct vfs_ioctx _kernel_ioctx = {0};

struct vfs_ioctx *const kernel_ioctx = &_kernel_ioctx;

mode_t vfs_vnode_to_mode(enum vnode_type type) {
    switch (type) {
    case VN_MNT:
    case VN_DIR:
        return S_IFDIR;
    case VN_LNK:
        return S_IFLNK;
    case VN_BLK:
        return S_IFBLK;
    case VN_CHR:
        return S_IFCHR;
    case VN_REG:
    default:
        return S_IFREG;
    }
}

int vfs_setcwd(struct vfs_ioctx *ctx, const char *path) {
    struct vnode *node;
    struct vnode *dst;
    int res;

    if (path[0] == '/') {
        if (path[1] == 0) {
            ctx->cwd_vnode = NULL;
            return 0;
        }

        if (!vfs_root) {
            ctx->cwd_vnode = NULL;
        } else {
            if ((res = vfs_find_internal(ctx, vfs_root, path, 0, &node)) != 0) {
                return res;
            }

            // Resolve mounts and symlinks
            if ((res = vfs_link_resolve(ctx, node, &dst, 0)) != 0) {
                return res;
            }

            if (dst->type == VN_MNT) {
                _assert(dst->target);
                dst = dst->target;
            }
            if (dst->type != VN_DIR) {
                return -ENOTDIR;
            }

            if ((res = vfs_access_node(ctx, dst, X_OK)) != 0) {
                return res;
            }

            ctx->cwd_vnode = dst;
        }
        return 0;
    } else {
        if ((res = vfs_find(ctx, ctx->cwd_vnode, path, 0, &node)) != 0) {
            return res;
        }

        if ((res = vfs_link_resolve(ctx, node, &dst, 0)) != 0) {
            return res;
        }

        if (dst->type == VN_MNT) {
            _assert(dst->target);
            dst = dst->target;
        }
        if (dst->type != VN_DIR) {
            return -ENOTDIR;
        }

        if ((res = vfs_access_node(ctx, dst, X_OK)) != 0) {
            return res;
        }

        ctx->cwd_vnode = dst;

        return 0;
    }
}

void vfs_vnode_path(char *path, struct vnode *node) {
    size_t c = 0;
    size_t off = 0;
    struct vnode *backstack[16] = { NULL };

    if (!node || !node->parent) {
        path[0] = '/';
        path[1] = 0;
        return;
    }

    // No need for root node
    while (node->parent) {
        _assert(c < 16);
        backstack[c++] = node;
        node = node->parent;
    }

    for (ssize_t i = c - 1; i >= 0; --i) {
        struct vnode *item = backstack[i];
        path[off++] = '/';
        if (item->type == VN_DIR && item->target) {
            // It's a filesystem root
            item = item->target;
        }

        strcpy(path + off, item->name);
        off += strlen(item->name);
    }
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

static int vfs_link_resolve_internal(struct vfs_ioctx *ctx,
                                     struct vnode *lnk,
                                     struct vnode **to,
                                     int link_itself,
                                     int cnt) {
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
        if ((res = vfs_find(ctx, lnk->parent, path, link_itself, &lnk->target)) != 0) {
            lnk->target = NULL;
            return res;
        }
    }

    struct vnode *target;
    if (lnk->flags & VN_PER_PROCESS) {
        _assert(lnk->target_func);
        // TODO: pass it as an argument instead of ioctx
        target = lnk->target_func(thread_self, lnk);
    } else {
        target = lnk->target;
    }
    if (target) {
        if (target->type == VN_LNK) {
            if (cnt <= 1) {
                return -ELOOP;
            }

            return vfs_link_resolve_internal(ctx, target, to, link_itself, cnt - 1);
        } else {
            *to = target;
            return 0;
        }
    }

    return -ENOENT;
}

int vfs_link_resolve(struct vfs_ioctx *ctx, struct vnode *lnk, struct vnode **to, int link_itself) {
    return vfs_link_resolve_internal(ctx, lnk, to, LINK_MAX, link_itself);
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

static int vfs_find_internal(struct vfs_ioctx *ctx,
                             struct vnode *at,
                             const char *path,
                             int link_itself,
                             struct vnode **child) {
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
        //kdebug("empty hit-final %s\n", at->name);
        return 0;
    }

    if (at->type == VN_LNK) {
        if ((err = vfs_link_resolve(ctx, at, &node, link_itself)) != 0) {
            //kdebug("miss: link resolution: %s\n", at->name);
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

    child_path = path;
    while (1) {
        child_path = path_element(child_path, name);

        if (!strcmp(name, ".")) {
            // Refers to this node
            //kdebug(". hit %s\n", at->name);
            if (!child_path || !*child_path) {
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
            //kdebug(".. hit %s\n", parent->name);
            return vfs_find_internal(ctx, parent, child_path, link_itself, child);
        }

        break;
    }

    if ((err = vfs_lookup_or_load(at, name, &node)) != 0) {
        //kdebug("miss %s in %s\n", name, at->name);
        return err;
    }

    _assert(node);

    if (!child_path) {
        //kdebug("hit-final %s\n", name);

        // TODO: only run link resolution here is asked to
        struct vnode *r = node;
        if (!link_itself) {
            if (r->type == VN_LNK) {
                if ((err = vfs_link_resolve(ctx, r, &node, 0)) != 0) {
                    //kdebug("miss: link resolution: %s\n", r->name);
                    return err;
                } else {
                    r = node;
                }
            }

            // TODO: links to links
            _assert(r->type != VN_LNK);
        }

        *child = r;
        return 0;
    } else {
        //kdebug("hit %s\n", name);
        return vfs_find_internal(ctx, node, child_path, link_itself, child);
    }

    return -ENOENT;
}

void vfs_init(void) {
    if (vfs_root) {
        vnode_dump_tree(DEBUG_INFO, vfs_root, 0);
    }
}

int vfs_umount(struct vfs_ioctx *ctx, const char *dir_name) {
    struct vnode *node;
    int res;

    if ((res = vfs_find(ctx, ctx->cwd_vnode, dir_name, 0, &node)) != 0) {
        return res;
    }

    if (!node->parent) {
        kwarn("Tried to umount root\n");
        return -EBUSY;
    }

    if (node->type != VN_DIR) {
        return -ENOTDIR;
    }

    if (!node->target) {
        return -EINVAL;
    }

    struct vnode *mountpoint = node->target;
    _assert(mountpoint->target = node);

    mountpoint->type = VN_DIR;
    mountpoint->target = NULL;

    struct fs *fs = node->fs;
    _assert(fs);
    struct blkdev *blk = fs->blk;
    _assert(blk);
    struct fs_class *cls = fs->cls;
    _assert(cls);

    if (cls->destroy) {
        cls->destroy(fs);
    }
    memset(fs, 0, sizeof(struct fs));
    blk->flags &= ~BLK_BUSY;

    // TODO: destroy filesystem tree

    return 0;
}

int vfs_mount_internal(struct vnode *at, void *blk, struct fs_class *cls, uint32_t flags, const char *opt) {
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
        if (at->target) {
            // This directory already has something mounted
            return -EBUSY;
        }
        if (at->type != VN_DIR) {
            return -ENOTDIR;
        }
    }

    if (!(fs = fs_create(cls, blk, flags, opt))) {
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
        fs_root->target = at;

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

int vfs_mount(struct vfs_ioctx *ctx, const char *at, void *blk, const char *fs, uint32_t flags, const char *opt) {
    int res;
    struct fs_class *fs_class;
    struct vnode *mountpoint;

    // NOTE: Mount uses only absolute paths
    if (at[0] != '/') {
        kwarn("Non-absolute path used for mounting\n");
        return -EINVAL;
    }

    if (!fs || !strcmp(fs, "auto")) {
        fs_class = NULL;
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
        if ((res = vfs_find(ctx, ctx->cwd_vnode, at, 0, &mountpoint)) != 0) {
            return res;
        }
    }

    if (!fs_class) {
        if (!blk) {
            return -EINVAL;
        }

        return blk_mount_auto(mountpoint, blk, flags, opt);
    } else {
        return vfs_mount_internal(mountpoint, blk, fs_class, flags, opt);
    }
}

int vfs_find(struct vfs_ioctx *ctx,
             struct vnode *rel,
             const char *path,
             int link_itself,
             struct vnode **node) {
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
        res = vfs_find_internal(ctx, vfs_root, path, link_itself, &r);
    } else {
        if (!rel) {
            rel = ctx->cwd_vnode;
        }
        if (!rel) {
            rel = vfs_root;
        }
        res = vfs_find_internal(ctx, rel, path, link_itself, &r);
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

