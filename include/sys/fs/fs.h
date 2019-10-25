#pragma once
#include "sys/fs/node.h"
#include "sys/blk.h"

// Means that the filesystem has a weird way of storing hierarchical
// nodes and employs a custom mapping algorithm. This ensures that
// at mount time the whole node tree for the filesystem is mapped
// to the VFS tree and the nodes do not get deallocated until the
// FS is unmounted.
// Note: when this flag is on, get_root is not used, as the filesystem
// produces the VFS tree all by itself.
#define FS_NODE_MAPPER          (1 << 0)

struct statvfs;
struct vfs_node;

struct fs_class {
    char name[256];

    int opt;

    vnode_t *(*get_root) (fs_t *fs);
    int (*mount) (fs_t *fs, const char *opt);
    int (*umount) (fs_t *fs);
    int (*statvfs) (fs_t *fs, struct statvfs *st);
    int (*mapper) (fs_t *fs, struct vfs_node **root);
};

// The actual filesystem instance,
// one per each mount
struct fs {
    // Mount details here
    vnode_t *mnt_at;

    // Block device on which the filesystem resides
    struct blkdev *blk;

    // Private data structure per-mount
    void *fs_private;

    struct fs_class *cls;
};

struct fs *fs_create(struct fs_class *cls, struct blkdev *blk, vnode_t *mnt_at);
void fs_release(struct fs *fs);
struct fs_class *fs_class_by_name(const char *name);
int fs_class_register(struct fs_class *cls);
