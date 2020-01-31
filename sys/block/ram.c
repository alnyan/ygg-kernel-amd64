#include "sys/user/errno.h"
#include "sys/block/ram.h"
#include "sys/block/blk.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/heap.h"
#include "sys/dev.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static struct ram_priv {
    uintptr_t begin;
    size_t lim;
} ram_priv;

static ssize_t ramblk_read(struct blkdev *blk, void *buf, size_t off, size_t len) {
    if (off > ram_priv.lim) {
        return -1;
    }

    size_t r = MIN(len, ram_priv.lim - off);
    memcpy(buf, (void *) (ram_priv.begin + off), r);
    return r;
}

static int ramblk_ioctl(struct blkdev *blk, unsigned long req, void *arg) {
    _assert(blk);

    switch (req) {
    case RAM_IOC_GETBASE:
        _assert(arg);
        *(uintptr_t *) arg = ram_priv.begin;
        return 0;
    default:
        return -EINVAL;
    }
}

static struct blkdev _ramblk0 = {
    .dev_data = &ram_priv,
    .read = ramblk_read,
    .ioctl = ramblk_ioctl,
    .write = NULL
};
struct blkdev *ramblk0 = &_ramblk0;

void ramblk_init(uintptr_t at, size_t len) {
    ram_priv.begin = at;
    ram_priv.lim = len;

    dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_RAM, &_ramblk0, "ram0");
//    struct dev_entry *ent = (struct dev_entry *) kmalloc(sizeof(struct dev_entry));
//    _assert(ent);
//
//    ent->dev = ramblk0;
//    ent->dev_class = DEV_CLASS_BLOCK;
//    ent->dev_subclass = DEV_BLOCK_RAM;
//    if (dev_alloc_name(ent->dev_class, ent->dev_subclass, ent->dev_name) != 0) {
//        panic("Failed to allocate a name for ram device\n");
//    }
//
//    dev_entry_add(ent);
}
