#pragma once
#include "sys/fs/node.h"
#include "sys/blk.h"

struct statvfs;

struct fs_class {
    char name[256];

    vnode_t *(*get_root) (fs_t *fs);
    int (*mount) (fs_t *fs, const char *opt);
    int (*umount) (fs_t *fs);
    int (*statvfs) (fs_t *fs, struct statvfs *st);

    struct vnode_operations op;
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
