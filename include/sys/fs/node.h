/** vim: ft=c.doxygen
 * @file node.h
 * @brief vnode
 */
#pragma once
#include <stdint.h>
#include "sys/types.h"
#include "dirent.h"
#include "sys/stat.h"

#define NODE_MAXLEN 256

struct ofile;
struct vfs_ioctx;
struct vnode;
typedef struct fs fs_t;

enum vnode_type {
    VN_REG,
    VN_DIR,
    VN_BLK,
    VN_CHR,
//    VN_LNK
};

struct vnode_operations {
    // File tree traversal, node instance operations
    int (*find) (struct vnode *node, const char *path, struct vnode **res);
    void (*destroy) (struct vnode *node);

    // Symlink
//    int (*readlink) (struct vnode *node, char *path);
//    int (*symlink) (struct vnode *at, struct vfs_ioctx *ctx, const char *name, const char *dst);

    // File entry operations
    int (*access) (struct vnode *node, uid_t *uid, gid_t *gid, mode_t *mode);
    int (*creat) (struct vnode *node, struct vfs_ioctx *ctx, const char *name, mode_t mode, int opt, struct vnode **res);
    int (*mkdir) (struct vnode *at, const char *name, mode_t mode);
    int (*unlink) (struct vnode *at, struct vnode *vn, const char *name);
    int (*stat) (struct vnode *node, struct stat *st);
    int (*chmod) (struct vnode *node, mode_t mode);
    int (*chown) (struct vnode *node, uid_t uid, gid_t gid);

    // Directory access
    int (*opendir) (struct vnode *node, int opt);
    int (*readdir) (struct ofile *fd);

    // File access
    int (*open) (struct vnode *node, int opt);
    void (*close) (struct ofile *fd);
    ssize_t (*read) (struct ofile *fd, void *buf, size_t count);
    ssize_t (*write) (struct ofile *fd, const void *buf, size_t count);
    int (*truncate) (struct ofile *fd, size_t length);
};

struct vnode {
    enum vnode_type type;

    char name[NODE_MAXLEN];

    struct vnode *first_child;
    struct vnode *next_child;
    struct vnode *parent;

    uint32_t refcount;
    uint64_t ino;

    fs_t *fs;
    void *fs_data;
    /*
     * (struct blkdev *) if type == VN_BLK
     * (struct chrdev *) if type == VN_CHR
     */
    void *dev;

    struct vnode_operations *op;
};

struct vnode *vnode_create(const char *name);
