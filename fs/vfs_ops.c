#include "user/errno.h"
#include "user/fcntl.h"
#include "sys/block/blk.h"
#include "sys/char/chr.h"
#include "fs/ofile.h"
#include "fs/node.h"
#include "fs/vfs.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"

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
    _assert(!ofile_is_socket(fd));

    struct vnode *node = fd->file.vnode;

    _assert(node);

    if (node->type != VN_DIR) {
        return -ENOTDIR;
    }

    if (node->flags & VN_MEMORY) {
        // Filesystem doesn't have to implement opendir/readdir for these - they're all
        // in memory and can be easily iterated
        fd->flags |= OF_MEMDIR | OF_MEMDIR_DOT;
        fd->file.pos = (size_t) node->first_child;

        ++node->open_count;

        return 0;
    } else {
        int res;
        if (!node->op || !node->op->opendir) {
            return -EINVAL;
        }

        if ((res = node->op->opendir(fd)) != 0) {
            return res;
        }

        ++node->open_count;

        return 0;
    }
}

int vfs_readdir(struct vfs_ioctx *ctx, struct ofile *fd, struct dirent *ent) {
    _assert(ctx);
    _assert(fd);
    _assert(!ofile_is_socket(fd));

    if (!(fd->flags & OF_DIRECTORY)) {
        return -ENOTDIR;
    }

    if (fd->flags & OF_MEMDIR) {
        // Kind of hacky but works
        if (fd->flags & OF_MEMDIR_DOT) {
            struct vnode *node = fd->file.vnode;
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
            struct vnode *node = fd->file.vnode;
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

        struct vnode *item = (struct vnode *) fd->file.pos;
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

        fd->file.pos = (size_t) item->next_child;

        return ent->d_reclen;
    } else {
        struct vnode *node = fd->file.vnode;
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
#if defined(ENABLE_NET)
    // ??
    fd->flags &= ~OF_SOCKET;
#endif

    if (node->type == VN_SOCK) {
        return -ENODEV;
    }

    if (opt & O_DIRECTORY) {
        if (node->type != VN_DIR) {
            return -ENOTDIR;
        }

        if ((opt & O_ACCMODE) != O_RDONLY) {
            return -EACCES;
        }

        fd->file.pos = 0;
        fd->file.vnode = node;
        fd->flags = OF_DIRECTORY | OF_READABLE;

        if ((res = vfs_opendir(ctx, fd)) != 0) {
            return res;
        }

        return 0;
    } else if (node->type == VN_DIR || node->type == VN_MNT) {
        return -EISDIR;
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

    fd->file.pos = 0;
    fd->file.vnode = node;
    fd->flags = 0;

    // Check for any unknown flags
    if ((opt & ~O_ACCMODE) & ~(O_CLOEXEC | O_CREAT | O_TRUNC)) {
        panic("Maybe I've forgot to handle this flag? opt = %x\n", opt & ~(O_ACCMODE | O_CLOEXEC));
    }
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
    if (opt & O_CLOEXEC) {
        fd->flags |= OF_CLOEXEC;
    }

    // 1. If file operations struct specifies some non-trivial open()
    //    function for the node - use it
    if (node->op && node->op->open) {
        if ((res = node->op->open(fd, opt)) != 0) {
            return res;
        }
    }

    ++fd->file.vnode->open_count;

    return 0;
}

int vfs_mkdirat(struct vfs_ioctx *ctx, struct vnode *rel, const char *path, mode_t mode) {
    char parent_path[PATH_MAX];
    const char *filename;
    int res;
    struct vnode *at;

    if (!rel) {
        rel = ctx->cwd_vnode;
    }

    _assert(ctx);
    _assert(path);

    filename = vfs_path_parent(path, parent_path);
    if (filename[0] == 0) {
        return -ENOENT;
    }

    // Find parent vnode
    if ((res = vfs_find(ctx, rel, parent_path, 0, &at)) != 0) {
        return res;
    }

    // Check permission for writing
    if ((res = vfs_access_node(ctx, at, W_OK)) != 0) {
        return res;
    }

    // TODO: check if such file already exists
    if (!at->op || !at->op->mkdir) {
        // Assume it's a read-only filesystem
        return -EROFS;
    }

    return at->op->mkdir(at, filename, ctx->uid, ctx->gid, mode & ~ctx->umask);
}

int vfs_unlinkat(struct vfs_ioctx *ctx, struct vnode *at, const char *pathname, int flags) {
    struct vnode *node;
    int res;

    if (!at) {
        at = ctx->cwd_vnode;
    }

    if ((res = vfs_find(ctx, at, pathname, 1, &node)) != 0) {
        return res;
    }

    if (node->type == VN_MNT) {
        return -EBUSY;
    }

    if (!node->op || !node->op->unlink) {
        return -EROFS;
    }

    if (node->open_count) {
        return -EBUSY;
    }

    if (flags & AT_REMOVEDIR) {
        if (node->type != VN_DIR) {
            return -ENOTDIR;
        }
        // TODO: check if directory is empty?
    } else {
        if (node->type == VN_DIR) {
            return -EISDIR;
        }
    }

    if ((res = node->op->unlink(node)) != 0) {
        return res;
    }

    vnode_detach(node);
    vnode_destroy(node);

    return 0;
}

int vfs_mknod(struct vfs_ioctx *ctx, const char *path, mode_t mode, struct vnode **_nod) {
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
    if ((res = vfs_find(ctx, ctx->cwd_vnode, parent_path, 0, &at)) != 0) {
        return res;
    }

    // Check permission for writing
    if ((res = vfs_access_node(ctx, at, W_OK)) != 0) {
        return res;
    }

    // TODO: check if such file already exists
    if (!at->op || !at->op->mknod) {
        // Assume it's a read-only filesystem
        return -EROFS;
    }

    enum vnode_type type;
    switch (mode & S_IFMT) {
    case S_IFIFO:
        type = VN_FIFO;
        break;
    case S_IFSOCK:
        type = VN_SOCK;
        break;
    default:
        panic("mknod: unsupported node type\n");
    }
    struct vnode *nod = vnode_create(type, filename);
    nod->mode = mode & VFS_MODE_MASK & ~ctx->umask;
    nod->uid = ctx->uid;
    nod->gid = ctx->gid;

    if ((res = at->op->mknod(at, nod)) != 0) {
        vnode_destroy(nod);
        return res;
    }

    *_nod = nod;

    return 0;
}

int vfs_creatat(struct vfs_ioctx *ctx,
                struct vnode *vn_at,
                const char *path,
                mode_t mode) {
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
    if ((res = vfs_find(ctx, vn_at, parent_path, 0, &at)) != 0) {
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

    return at->op->creat(at, filename, ctx->uid, ctx->gid, mode & ~ctx->umask);
}

int vfs_openat(struct vfs_ioctx *ctx,
               struct ofile *fd,
               struct vnode *at,
               const char *path,
               int opt, int mode) {
    if (!at) {
        at = ctx->cwd_vnode;
    }
    struct vnode *node;
    int res;

    _assert(ctx);
    _assert(fd);
    _assert(path);

    if ((res = vfs_find(ctx, at, path, 0, &node)) != 0) {
        if (opt & O_CREAT) {
            if (opt & O_DIRECTORY) {
                return -EINVAL;
            }

            // Try to create a new file
            if ((res = vfs_creatat(ctx, at, path, mode)) != 0) {
                return res;
            }

            // If creation succeeded, find file again
            if ((res = vfs_find(ctx, at, path, 0, &node)) != 0) {
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
    _assert(!ofile_is_socket(fd));
    _assert(!(fd->refcount));
    _assert(fd->file.vnode);

    --fd->file.vnode->open_count;

    if (fd->file.vnode->op && fd->file.vnode->op->close) {
        fd->file.vnode->op->close(fd);
    }
}

int vfs_faccessat(struct vfs_ioctx *ctx, struct vnode *at, const char *path, int mode, int flags) {
    _assert(ctx);
    _assert(path);

    struct vnode *node;
    int res;

    if ((res = vfs_find(ctx, at, path, flags & AT_SYMLINK_NOFOLLOW, &node)) != 0) {
        return res;
    }

    if (mode == F_OK) {
        return 0;
    }

    return vfs_access_node(ctx, node, mode);
}

int vfs_fstatat(struct vfs_ioctx *ctx, struct vnode *at, const char *path, struct stat *st, int flags) {
    struct vnode *node;
    int res;

    _assert(ctx);
    _assert(st);

    if (!at) {
        at = ctx->cwd_vnode;
    }

    if (flags & AT_EMPTY_PATH) {
        node = at;

        if (!node) {
            panic("TODO: implement this (use root)\n");
        }
    } else {
        _assert(path);

        if ((res = vfs_find(ctx, at, path, flags & AT_SYMLINK_NOFOLLOW, &node)) != 0) {
            return res;
        }
    }

    if (!node->op || !node->op->stat) {
        kerror("stat() not implemented for node\n");
        return -EINVAL;
    }

    return node->op->stat(node, st);
}

ssize_t vfs_readlinkat(struct vfs_ioctx *ctx,
                       struct vnode *at,
                       const char *restrict pathname,
                       char *restrict buf,
                       size_t lim) {
    struct vnode *node;
    int res;

    _assert(pathname);
    _assert(ctx);
    _assert(buf);

    if (!at) {
        at = ctx->cwd_vnode;
    }

    if ((res = vfs_find(ctx, at, pathname, AT_SYMLINK_NOFOLLOW, &node)) != 0) {
        return res;
    }

    if (node->type != VN_LNK) {
        return -EINVAL;
    }

    if (!node->op || !node->op->readlink) {
        kerror("readlink() not implemented for node\n");
        return -EINVAL;
    }

    return node->op->readlink(node, buf, lim);
}

int vfs_ftruncate(struct vfs_ioctx *ctx, struct vnode *node, off_t length) {
    _assert(ctx);
    _assert(node);

    if (node->type != VN_REG) {
        kwarn("truncate() on non-file\n");
        return -EPERM;
    }

    if (node->op && node->op->truncate) {
        return node->op->truncate(node, length);
    } else {
        return -ENOSYS;
    }
}

ssize_t vfs_write(struct vfs_ioctx *ctx, struct ofile *fd, const void *buf, size_t count) {
    struct vnode *node;
    ssize_t b;

    _assert(fd);
    _assert(!ofile_is_socket(fd));

    if (!(fd->flags & OF_WRITABLE) || (fd->flags & OF_DIRECTORY)) {
        return -EINVAL;
    }

    node = fd->file.vnode;

    _assert(ctx);
    _assert(buf);
    _assert(node);

    switch (node->type) {
    case VN_REG:
    case VN_FIFO:
        _assert(node->op && node->op->write);
        b = node->op->write(fd, buf, count);
        return b;
    case VN_CHR:
        _assert(node->dev && ((struct chrdev *) node->dev)->write);
        return chr_write(node->dev, buf, fd->file.pos, count);

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
    _assert(!ofile_is_socket(fd));

    node = fd->file.vnode;

    _assert(ctx);
    _assert(buf);
    _assert(node);

    switch (node->type) {
    case VN_REG:
    case VN_FIFO:
        _assert(node->op && node->op->read);
        b = node->op->read(fd, buf, count);
        return b;
    case VN_CHR:
        _assert(node->dev && ((struct chrdev *) node->dev)->read);
        return chr_read(node->dev, buf, fd->file.pos, count);
    case VN_BLK:
        _assert(node->dev);
        b = blk_read(node->dev, buf, fd->file.pos, count);
        if (b > 0) {
            fd->file.pos += b;
        }
        return b;

    case VN_DIR:
    default:
        // Cannot do such things
        return -EINVAL;
    }
}

off_t vfs_lseek(struct vfs_ioctx *ctx, struct ofile *fd, off_t offset, int whence) {
    _assert(fd);
    _assert(!ofile_is_socket(fd));
    struct vnode *node = fd->file.vnode;
    _assert(node);

    if (node->type != VN_REG) {
        return -ESPIPE;
    }

    if (!node->op || !node->op->lseek) {
        return -EINVAL;
    }

    return node->op->lseek(fd, offset, whence);
}

int vfs_chmod(struct vfs_ioctx *ctx, const char *path, mode_t mode) {
    _assert(ctx);
    _assert(path);
    struct vnode *node;
    int res;

    mode &= VFS_MODE_MASK;

    if ((res = vfs_find(ctx, ctx->cwd_vnode, path, 1, &node)) != 0) {
        return res;
    }

    if (node->type == VN_LNK) {
        kwarn("Tried to change permissions on a symbolic link\n");
        return -EINVAL;
    }

    // Only root or file owner can do that
    if (ctx->uid != 0 && ctx->uid != node->uid) {
        return -EACCES;
    }

    if (!node->op || !node->op->chmod) {
        if (node->flags & VN_MEMORY) {
            node->mode &= ~VFS_MODE_MASK;
            node->mode |= mode & VFS_MODE_MASK;
            return 0;
        } else {
            return -EINVAL;
        }
    }

    if ((res = node->op->chmod(node, mode)) != 0) {
        return res;
    }

    node->mode = mode;

    return 0;
}

int vfs_chown(struct vfs_ioctx *ctx, const char *path, uid_t uid, gid_t gid) {
    struct vnode *node;
    int res;

    if ((res = vfs_find(ctx, ctx->cwd_vnode, path, 0, &node)) != 0) {
        return res;
    }

    // Only root can chown
    if (ctx->uid != 0) {
        return -EACCES;
    }

    if (!node->op || !node->op->chown) {
        if (!(node->flags & VN_MEMORY)) {
            return -EINVAL;
        }
    } else {
        if ((res = node->op->chown(node, uid, gid)) != 0) {
            return res;
        }
    }

    node->uid = uid;
    node->gid = gid;

    return 0;
}

int vfs_ioctl(struct vfs_ioctx *ctx, struct ofile *fd, unsigned int cmd, void *arg) {
    _assert(ctx);
    _assert(fd);
    _assert(!ofile_is_socket(fd));
    struct vnode *node = fd->file.vnode;
    _assert(node);

    switch (node->type) {
    case VN_CHR:
        return chr_ioctl(node->dev, cmd, arg);
    case VN_BLK:
        return blk_ioctl(node->dev, cmd, arg);
    default:
        return -EINVAL;
    }
}
