#pragma once
#include <stdint.h>
#include <sys/types.h>

struct blkdev {
    void *dev_data;

    ssize_t (*read) (struct blkdev *blk, void *buf, size_t off, size_t count);
    ssize_t (*write) (struct blkdev *blk, const void *buf, size_t off, size_t count);

    void (*destroy) (struct blkdev *blk);
};

ssize_t blk_read(struct blkdev *blk, void *buf, size_t off, size_t count);
ssize_t blk_write(struct blkdev *blk, const void *buf, size_t off, size_t count);
