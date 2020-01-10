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
    VN_UNK,
};

struct vnode_operations {
    int (*open) (struct ofile *fd);
    void (*close) (struct ofile *fd);

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

    uint32_t open_count;
    uint64_t ino;

    gid_t gid;
    uid_t uid;

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
int vnode_lookup_tree(struct vnode *at, const char *rel_path, struct vnode **child);

