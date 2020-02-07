#pragma once
#include "sys/types.h"

struct statvfs;
struct blkdev;
struct vnode;
struct fs;

struct fs_class {
    char name[64];

    int opt;

    struct vnode *(*get_root) (struct fs *fs);
    int (*init) (struct fs *fs, const char *opt);
    void (*destroy) (struct fs *fs);
    int (*statvfs) (struct fs *fs, struct statvfs *st);
};

// The actual filesystem instance,
// one per each mount
struct fs {
    // Block device on which the filesystem resides
    struct blkdev *blk;

    uint32_t flags;

    // Private data structure per-mount
    void *fs_private;

    struct fs_class *cls;
};

struct fs *fs_create(struct fs_class *cls, struct blkdev *blk, uint32_t flags, const char *opt);
void fs_release(struct fs *fs);
struct fs_class *fs_class_by_name(const char *name);
int fs_class_register(struct fs_class *cls);
