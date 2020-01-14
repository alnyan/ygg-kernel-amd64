#include "sys/fs/ofile.h"
#include "sys/fs/node.h"
#include "sys/fs/vfs.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/fcntl.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/chr.h"

static int vfs_open_mode(int opt) {
    int r = 0;
    if (opt & O_EXEC) {
        r |= X_OK;
    }
    switch (opt & O_ACCMODE) {
    case O_WRONLY:
        r |= W_OK;
        break;
    case O_RDONLY:
        r |= R_OK;
        break;
    case O_RDWR:
        r |= W_OK | R_OK;
        break;
    }
    return r;
}

// Returns:
// Parent path in dst
// Child name in returned pointer
static const char *vfs_path_parent(const char *input, char *dst) {
    const char *p = strrchr(input, '/');

    if (!p) {
        // In paths like "filename.txt", assume parent is "."
        dst[0] = '.';
        dst[1] = 0;
        return input;
    } else {
        // Paths like "rel/path/file.txt"
        // or "/abs/path/file.txt"
        if (p == input) {
            dst[0] = '/';
            dst[1] = 0;
        } else {
            _assert(p - input < PATH_MAX);
            strncpy(dst, input, p - input);
            dst[p - input] = 0;
        }

        return p + 1;
    }
}

static int vfs_opendir(struct vfs_ioctx *ctx, struct ofile *fd) {
    _assert(ctx);
    _assert(fd);

    struct vnode *node = fd->vnode;

    _assert(node);

    if (node->type != VN_DIR) {
        return -ENOTDIR;
    }

    if (node->flags & VN_MEMORY) {
        // Filesystem doesn't have to implement opendir/readdir for these - they're all
        // in memory and can be easily iterated
        fd->flags |= OF_MEMDIR | OF_MEMDIR_DOT;
        fd->pos = (size_t) node->first_child;

        return 0;
    } else {
        if (!node->op || !node->op->opendir) {
            return -EINVAL;
        }

        return node->op->opendir(fd);
    }
}

int vfs_readdir(struct vfs_ioctx *ctx, struct ofile *fd, struct dirent *ent) {
    _assert(ctx);
    _assert(fd);

    if (!(fd->flags & OF_DIRECTORY)) {
        return -ENOTDIR;
    }

    if (fd->flags & OF_MEMDIR) {
        // Kind of hacky but works
        if (fd->flags & OF_MEMDIR_DOT) {
            struct vnode *node = fd->vnode;
            _assert(node);

            fd->flags &= ~OF_MEMDIR_DOT;
            fd->flags |= OF_MEMDIR_DOTDOT;

            ent->d_ino = node->ino;
            ent->d_type = DT_DIR;
            ent->d_off = 0;
            ent->d_name[0] = '.';
            ent->d_name[1] = 0;
            ent->d_reclen = sizeof(struct dirent) + 1;

            return ent->d_reclen;
        } else if (fd->flags & OF_MEMDIR_DOTDOT) {
            struct vnode *node = fd->vnode;
            _assert(node);
            if (node->parent) {
                node = node->parent;
            }

            fd->flags &= ~OF_MEMDIR_DOTDOT;

            ent->d_ino = node->ino;
            ent->d_type = DT_DIR;
            ent->d_off = 0;
            ent->d_name[0] = '.';
            ent->d_name[1] = '.';
            ent->d_name[2] = 0;
            ent->d_reclen = sizeof(struct dirent) + 2;

            return ent->d_reclen;
        }

        struct vnode *item = (struct vnode *) fd->pos;
        if (!item) {
            return -1;
        }

        // Fill dirent
        ent->d_ino = item->ino;
        ent->d_off = 0;

        switch (item->type) {
        case VN_REG:
            ent->d_type = DT_REG;
            break;
        case VN_DIR:
        case VN_MNT:
            ent->d_type = DT_DIR;
            break;
        default:
            ent->d_type = DT_UNKNOWN;
            break;
        }

        strcpy(ent->d_name, item->name);
        ent->d_reclen = sizeof(struct dirent) + strlen(item->name);

        fd->pos = (size_t) item->next_child;

        return ent->d_reclen;
    } else {
        struct vnode *node = fd->vnode;
        _assert(node);
        _assert(node->op && node->op->readdir);
        return node->op->readdir(fd, ent);
    }
}

int vfs_open_vnode(struct vfs_ioctx *ctx, struct ofile *fd, struct vnode *node, int opt) {
    int res;
    int accmode;

    _assert(ctx);
    _assert(fd);
    _assert(node);

    if (opt & O_DIRECTORY) {
        if ((opt & O_ACCMODE) != O_RDONLY) {
            return -EACCES;
        }

        fd->refcount = 0;
        fd->pos = 0;
        fd->vnode = node;
        fd->flags = OF_DIRECTORY | OF_READABLE;

        if ((res = vfs_opendir(ctx, fd)) != 0) {
            return res;
        }

        ++fd->refcount;
        return 0;
    }

    // Check node access
    accmode = vfs_open_mode(opt);
    if ((res = vfs_access_node(ctx, node, accmode)) != 0) {
        kwarn("Access failed: %s\n", node->name);
        return res;
    }

    // Truncate the file if requested
    if (opt & O_TRUNC) {
        if (!node->op || !node->op->truncate) {
            return -EINVAL;
        }

        if ((res = node->op->truncate(node, 0)) != 0) {
            return res;
        }
    }

    fd->refcount = 0;
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

    // 1. If file operations struct specifies some non-trivial open()
    //    function for the node - use it
    if (node->op && node->op->open) {
        if ((res = node->op->open(fd, opt)) != 0) {
            return res;
        }
    }

    ++fd->refcount;

    return 0;
}

int vfs_creat(struct vfs_ioctx *ctx, const char *path, mode_t mode) {
    char parent_path[PATH_MAX];
    const char *filename;
    int res;
    struct vnode *at;

    _assert(ctx);
    _assert(path);

    filename = vfs_path_parent(path, parent_path);
    if (filename[0] == 0) {
        return -ENOENT;
    }

    // Find parent vnode
    if ((res = vfs_find(ctx, ctx->cwd_vnode, parent_path, &at)) != 0) {
        return res;
    }

    // Check permission for writing
    if ((res = vfs_access_node(ctx, at, W_OK)) != 0) {
        return res;
    }

    // TODO: check if such file already exists
    if (!at->op || !at->op->creat) {
        // Assume it's a read-only filesystem
        return -EROFS;
    }

    return at->op->creat(at, filename, ctx->uid, ctx->gid, mode);
}

int vfs_open(struct vfs_ioctx *ctx, struct ofile *fd, const char *path, int opt, int mode) {
    struct vnode *node;
    int res;

    _assert(ctx);
    _assert(fd);
    _assert(path);

    if ((res = vfs_find(ctx, ctx->cwd_vnode, path, &node)) != 0) {
        if (opt & O_CREAT) {
            if (opt & O_DIRECTORY) {
                return -EINVAL;
            }

            // Try to create a new file
            if ((res = vfs_creat(ctx, path, mode)) != 0) {
                return res;
            }

            // If creation succeeded, find file again
            if ((res = vfs_find(ctx, ctx->cwd_vnode, path, &node)) != 0) {
                return res;
            }
        } else {
            return res;
        }
    }

    return vfs_open_vnode(ctx, fd, node, opt);
}

void vfs_close(struct vfs_ioctx *ctx, struct ofile *fd) {
    _assert(ctx);
    _assert(fd);
    _assert(fd->vnode);

    if (fd->vnode->op && fd->vnode->op->close) {
        fd->vnode->op->close(fd);
    }

    --fd->refcount;
}

int vfs_access(struct vfs_ioctx *ctx, const char *path, int accmode) {
    struct vnode *node;
    int res;

    _assert(ctx);
    _assert(path);

    if ((res = vfs_find(ctx, ctx->cwd_vnode, path, &node)) != 0) {
        return res;
    }

    if (accmode == F_OK) {
        // Just test that file exists
        return 0;
    }

    return vfs_access_node(ctx, node, accmode);
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
        return -EINVAL;
    }

    return node->op->stat(node, st);
}

ssize_t vfs_write(struct vfs_ioctx *ctx, struct ofile *fd, const void *buf, size_t count) {
    struct vnode *node;
    ssize_t b;

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
        b = node->op->write(fd, buf, count);
        if (b > 0) {
            fd->pos += b;
        }
        return b;
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
    ssize_t b;

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
        b = node->op->read(fd, buf, count);
        if (b > 0) {
            fd->pos += b;
        }
        return b;
    case VN_CHR:
        _assert(node->dev && ((struct chrdev *) node->dev)->read);
        return chr_read(node->dev, buf, fd->pos, count);

    case VN_DIR:
    default:
        // Cannot do such things
        return -EINVAL;
    }
}

off_t vfs_lseek(struct vfs_ioctx *ctx, struct ofile *fd, off_t offset, int whence) {
    _assert(fd);
    struct vnode *node = fd->vnode;
    _assert(node);

    if (node->type != VN_REG) {
        return -EINVAL;
    }

    if (!node->op || !node->op->lseek) {
        return -EINVAL;
    }

    return node->op->lseek(fd, offset, whence);
}
