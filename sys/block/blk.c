#include "sys/block/part_gpt.h"
#include "user/errno.h"
#include "sys/block/blk.h"
#include "fs/node.h"
#include "sys/assert.h"
#include "fs/vfs.h"
#include "sys/string.h"
#include "fs/fs.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/dev.h"
#include "sys/mm.h"

static struct block_cache *g_cache_head = NULL, *g_cache_tail = NULL;

struct blk_part {
    struct blkdev *device;
    uint64_t lba_start;
    uint64_t size;
};

void blk_set_cache(struct blkdev *blk, size_t page_capacity) {
    _assert(blk);
    _assert(!(blk->flags & BLK_CACHE));

    block_cache_init(&blk->cache, blk, MM_PAGE_SIZE, page_capacity);

    if (g_cache_tail) {
        g_cache_tail->g_next = &blk->cache;
    } else {
        g_cache_head = &blk->cache;
    }
    blk->cache.g_prev = g_cache_tail;
    blk->cache.g_next = NULL;
    g_cache_tail = &blk->cache;

    blk->flags |= BLK_CACHE;
}

void blk_sync_all(void) {
    kdebug("Global cache sync\n");
    for (struct block_cache *cache = g_cache_head; cache; cache = cache->g_next) {
        block_cache_flush(cache);
    }
}

void *blk_mmap(struct blkdev *blk, struct ofile *of, void *hint, size_t length, int flags) {
    _assert(blk);
    if (blk->mmap) {
        return blk->mmap(blk, of, hint, length, flags);
    } else {
        return (void *) -EINVAL;
    }
}

static inline ssize_t blk_read_really(struct blkdev *blk, void *buf, size_t off, size_t lim) {
    if (blk->read) {
        return blk->read(blk, buf, off, lim);
    } else {
        return -EINVAL;
    }
}

ssize_t blk_read(struct blkdev *blk, void *buf, size_t off, size_t lim) {
    _assert(blk);

    if (blk->flags & BLK_CACHE) {
        // Use cache to fetch pages
        uintptr_t page;
        size_t page_size = blk->cache.page_size;
        size_t rem = lim;
        size_t index = off / page_size;
        size_t bread = 0;
        int err;

        while (rem) {
            size_t blk_off = off % page_size;
            size_t can = MIN(page_size - blk_off, rem);

            if (block_cache_get(&blk->cache, index * page_size, &page) != 0) {
                // Fetch page from device
                if ((err = blk_read_really(blk, (void *) MM_VIRTUALIZE(page), index * page_size, page_size)) < 0) {
                    kerror("Read failed: %s\n", kstrerror(err));
                    panic("TODO: deal with to-cache reads\n");
                }
            }

            memcpy(buf, (void *) MM_VIRTUALIZE(page + blk_off), can);

            rem -= can;
            buf += can;
            bread += can;
            ++index;
        }

        return bread;
    } else {
        // Bypass the cache
        return blk_read_really(blk, buf, off, lim);
    }
}

static inline ssize_t blk_write_really(struct blkdev *blk, const void *buf, size_t off, size_t lim) {
    if (blk->write) {
        return blk->write(blk, buf, off, lim);
    } else {
        return -EINVAL;
    }
}

int blk_page_sync(struct blkdev *blk, uintptr_t block_address, uintptr_t page) {
    ssize_t res = blk_write_really(blk, (void *) MM_VIRTUALIZE(page), block_address, blk->cache.page_size);
    if (res < 0) {
        return res;
    } else {
        return ((size_t) res == blk->cache.page_size) ? 0 : -EIO;
    }
}

ssize_t blk_write(struct blkdev *blk, const void *buf, size_t off, size_t lim) {
    _assert(blk);
    if (blk->flags & BLK_CACHE) {
        uintptr_t page;
        size_t page_size = blk->cache.page_size;
        size_t rem = lim;
        size_t index = off / page_size;
        size_t bwritten = 0;
        int err;

        while (rem) {
            size_t blk_off = off % page_size;
            size_t can = MIN(page_size - blk_off, rem);

            if (block_cache_get(&blk->cache, index * page_size, &page) != 0) {
                // Fetch page
                if ((err = blk_read_really(blk, (void *) MM_VIRTUALIZE(page), index * page_size, page_size)) < 0) {
                    kerror("Read failed: %s\n", kstrerror(err));
                    panic("TODO: deal with to-cache reads\n");
                }
            }

            memcpy((void *) MM_VIRTUALIZE(page + blk_off), buf, can);
            block_cache_mark_dirty(&blk->cache, index * page_size);

            rem -= can;
            buf += can;
            bwritten += can;
            ++index;
        }

        return bwritten;
    } else {
        return blk_write_really(blk, buf, off, lim);
    }
}

int blk_ioctl(struct blkdev *blk, unsigned long req, void *arg) {
    _assert(blk);

    if (blk->ioctl) {
        return blk->ioctl(blk, req, arg);
    } else {
        return -EINVAL;
    }
}

int blk_mount_auto(struct vnode *at, struct blkdev *blk, const char *opt) {
    struct fs_class *cls;
    int res;

    if ((cls = fs_class_by_name("ext2")) != NULL &&
        (res = vfs_mount_internal(at, blk, cls, opt)) == 0) {
        return 0;
    }

    return -EINVAL;
}

static ssize_t blk_part_write(struct blkdev *dev, const void *buf, size_t off, size_t count) {
    struct blk_part *part = (struct blk_part *) dev->dev_data;
    size_t part_size_bytes = part->size * 512;
    size_t part_off_bytes = part->lba_start * 512;

    if (off >= part_size_bytes) {
        return -1;
    }

    size_t l = MIN(part_size_bytes - off, count);

    return blk_write(part->device, buf, off + part_off_bytes, l);
}

static ssize_t blk_part_read(struct blkdev *dev, void *buf, size_t off, size_t count) {
    struct blk_part *part = (struct blk_part *) dev->dev_data;
    size_t part_size_bytes = part->size * 512;
    size_t part_off_bytes = part->lba_start * 512;

    if (off >= part_size_bytes) {
        return -1;
    }

    size_t l = MIN(part_size_bytes - off, count);

    return blk_read(part->device, buf, off + part_off_bytes, l);
}

int blk_add_part(struct vnode *of, int n, uint64_t lba, uint64_t size) {
    // Allocate both a device and partition details
    char *buf = kmalloc(sizeof(struct blk_part) + sizeof(struct blkdev));
    _assert(buf);
    struct blk_part *part = (struct blk_part *) (buf + sizeof(struct blkdev));
    struct blkdev *dev = (struct blkdev *) buf;

    dev->dev_data = part;
    dev->read = blk_part_read;
    dev->write = blk_part_write;
    dev->flags = 0;

    part->device = of->dev;
    part->lba_start = lba;
    part->size = size;

    char name[16];
    size_t len = strlen(of->name);
    _assert(len < sizeof(name) - 2);
    strncpy(name, of->name, len);
    name[len] = '1' + n;
    name[len + 1] = 0;

    dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_PART, dev, name);

    return 0;
}

int blk_enumerate_partitions(struct blkdev *blk, struct vnode *node) {
    // TODO: better differentiation for CD/DVD-drives than just assuming every
    //       drive with block size of 2048 is one
    ssize_t res;
    uint8_t buf[1024];

    if (blk->block_size == 512) {
        if ((res = blk_read(blk, buf, 0, 1024)) < 0) {
            return res;
        }

        if (!strncmp((const char *) (buf + 512), "EFI PART", 8)) {
            return blk_enumerate_gpt(blk, node, buf);
        }
    }
    return 0;
}
