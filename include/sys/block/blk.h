#pragma once
#include "sys/block/cache.h"
#include "sys/types.h"

#define BLK_CACHE       (1 << 1)
#define BLK_BUSY        (1 << 0)

struct vnode;
struct ofile;

struct blkdev {
    uint32_t flags;
    size_t block_size;
    struct block_cache cache;

    void *dev_data;

    ssize_t (*read) (struct blkdev *blk, void *buf, size_t off, size_t count);
    ssize_t (*write) (struct blkdev *blk, const void *buf, size_t off, size_t count);
    int (*ioctl) (struct blkdev *blk, unsigned long req, void *ptr);

    int (*mmap)(struct blkdev *blk, uintptr_t base, size_t page_count, int prot, int flags);

    void (*destroy) (struct blkdev *blk);
};

void blk_set_cache(struct blkdev *blk, size_t capacity);
void blk_cache_release(struct blkdev *blk);
void blk_sync(struct blkdev *blk);
int blk_page_sync(struct blkdev *blk, uintptr_t address, uintptr_t page);
void blk_sync_all(void);

int blk_mmap(struct blkdev *blk, uintptr_t base, size_t page_count, int prot, int flags);
ssize_t blk_read(struct blkdev *blk, void *buf, size_t off, size_t count);
ssize_t blk_write(struct blkdev *blk, const void *buf, size_t off, size_t count);
int blk_ioctl(struct blkdev *blk, unsigned long req, void *ptr);
int blk_mount_auto(struct vnode *at, struct blkdev *blkdev, uint32_t flags, const char *opt);

int blk_add_part(struct vnode *of, int n, uint64_t lba, uint64_t size);
int blk_enumerate_partitions(struct blkdev *blk, struct vnode *node);
