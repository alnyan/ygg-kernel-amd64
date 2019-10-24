#pragma once
#include "sys/types.h"
#include "sys/dev.h"

struct blkdev {
    void *dev_data;
    struct dev_entry *ent;
    struct dev_entry *ent_parent;

    ssize_t (*read) (struct blkdev *blk, void *buf, size_t off, size_t count);
    ssize_t (*write) (struct blkdev *blk, const void *buf, size_t off, size_t count);

    void (*destroy) (struct blkdev *blk);
};

ssize_t blk_read(struct blkdev *blk, void *buf, size_t off, size_t count);
ssize_t blk_write(struct blkdev *blk, const void *buf, size_t off, size_t count);

struct blkdev *blk_by_name(const char *name);
int blk_enumerate_partitions(struct blkdev *blk);
