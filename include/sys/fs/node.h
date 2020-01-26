/** vim: ft=c.doxygen
 * @file node.h
 * @brief vnode
 */
#pragma once
#include <stdint.h>
#include "sys/types.h"
#include "dirent.h"
#include "sys/stat.h"

#define NODE_MAXLEN 64

// Means the node has no physical storage and resides only
// in memory
#define VN_MEMORY       (1 << 0)

struct ofile;
struct vfs_ioctx;
struct vnode;
struct fs;

enum vnode_type {
    VN_REG,
    VN_DIR,
    VN_BLK,
    VN_CHR,
    VN_LNK,
    VN_UNK,
    VN_MNT,
};

struct vnode_operations {
    int (*find) (struct vnode *at, const char *name, struct vnode **node);

    ssize_t (*readdir) (struct ofile *fd, struct dirent *ent);
    int (*opendir) (struct ofile *fd);
    int (*open) (struct ofile *fd, int opt);
    void (*close) (struct ofile *fd);
    int (*creat) (struct vnode *at, const char *filename, uid_t uid, gid_t gid, mode_t mode);
    int (*mkdir) (struct vnode *at, const char *filename, uid_t uid, gid_t gid, mode_t mode);
    int (*truncate) (struct vnode *at, size_t size);
    int (*unlink) (struct vnode *vn);

    off_t (*lseek) (struct ofile *fd, off_t offset, int whence);

    int (*stat) (struct vnode *at, struct stat *st);
    int (*chmod) (struct vnode *node, mode_t mode);
    int (*chown) (struct vnode *node, uid_t uid, gid_t gid);

    int (*readlink) (struct vnode *at, char *buf, size_t lim);

    ssize_t (*read) (struct ofile *fd, void *buf, size_t count);
    ssize_t (*write) (struct ofile *fd, const void *buf, size_t count);
};

struct vnode {
    enum vnode_type type;

    char name[NODE_MAXLEN];
    uint32_t flags;

    struct vnode *first_child;
    struct vnode *next_child;
    struct vnode *parent;

    // For filesystem roots, mountpoint directory vnode
    // For symlinks, this is target vnode
    // For mountpoints, this is filesystem root
    struct vnode *target;

    uint32_t open_count;
    uint64_t ino;

    gid_t gid;
    uid_t uid;
    mode_t mode;

    struct fs *fs;
    void *fs_data;
    /*
     * (struct blkdev *) if type == VN_BLK
     * (struct chrdev *) if type == VN_CHR
     */
    void *dev;

    struct vnode_operations *op;
};

// Node itself
struct vnode *vnode_create(enum vnode_type t, const char *name);

// Tree manipulation
void vnode_attach(struct vnode *parent, struct vnode *child);
void vnode_detach(struct vnode *node);

// Tree traversal
void vnode_dump_tree(int level, struct vnode *node, int depth);
int vnode_lookup_child(struct vnode *of, const char *name, struct vnode **child);
