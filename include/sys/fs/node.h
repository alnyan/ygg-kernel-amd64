/** vim: ft=c.doxygen
 * @file node.h
 * @brief vnode
 */
#pragma once
#include <stdint.h>
#include "sys/types.h"
#include "dirent.h"
#include "sys/stat.h"

struct ofile;
struct vfs_ioctx;
typedef struct vnode vnode_t;
typedef struct fs fs_t;

enum vnode_type {
    VN_REG,
    VN_DIR,
    VN_BLK,
    VN_CHR,
    VN_LNK
};

/**
 * @brief Set of functions implemented by filesystem driver
 */
struct vnode_operations {
    // File tree traversal, node instance operations
    int (*find) (vnode_t *node, const char *path, vnode_t **res);
    void (*destroy) (vnode_t *node);

    // Symlink
    int (*readlink) (vnode_t *node, char *path);
    int (*symlink) (vnode_t *at, struct vfs_ioctx *ctx, const char *name, const char *dst);

    // File entry operations
    int (*access) (vnode_t *node, uid_t *uid, gid_t *gid, mode_t *mode);
    int (*creat) (vnode_t *node, struct vfs_ioctx *ctx, const char *name, mode_t mode, int opt, vnode_t **res);
    int (*mkdir) (vnode_t *at, const char *name, mode_t mode);
    int (*unlink) (vnode_t *at, vnode_t *vn, const char *name);
    int (*stat) (vnode_t *node, struct stat *st);
    int (*chmod) (vnode_t *node, mode_t mode);
    int (*chown) (vnode_t *node, uid_t uid, gid_t gid);

    // Directory access
    int (*opendir) (vnode_t *node, int opt);
    int (*readdir) (struct ofile *fd);

    // File access
    int (*open) (vnode_t *node, int opt);
    void (*close) (struct ofile *fd);
    ssize_t (*read) (struct ofile *fd, void *buf, size_t count);
    ssize_t (*write) (struct ofile *fd, const void *buf, size_t count);
    int (*truncate) (struct ofile *fd, size_t length);
};

struct vnode {
    enum vnode_type type;

    uint32_t refcount;

    fs_t *fs;
    // Private filesystem-specific data (like inode struct)
    void *fs_data;
    // Private filesystem-specific number (like inode number)
    uint32_t fs_number;

    /*
     * (struct blkdev *) if type == VN_BLK
     * (struct chrdev *) if type == VN_CHR
     */
    void *dev;
    void *tree_node;

    struct vnode_operations *op;
};

void vnode_ref(vnode_t *vn);
void vnode_unref(vnode_t *vn);
void vnode_free(vnode_t *vn);
