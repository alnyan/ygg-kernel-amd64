#include "sys/string.h"
#include "sys/panic.h"
#include "sys/attr.h"
#include "sys/blk.h"
#include "sys/dev.h"

#define DEV_ZERO        0
#define DEV_NULL        1

static ssize_t pseudo_write(struct blkdev *dev, const void *buf, size_t pos, size_t lim);
static ssize_t pseudo_read(struct blkdev *dev, void *buf, size_t pos, size_t lim);

static struct blkdev _dev_null = {
    .dev_data = (void *) DEV_NULL,
    .write = pseudo_write,
    .read = pseudo_read
};
static struct dev_entry _ent_null = {
    .dev = &_dev_null,
    .dev_class = DEV_CLASS_BLOCK,
    .dev_subclass = DEV_BLOCK_PSEUDO,
    .dev_name = "null"
};
static struct blkdev _dev_zero = {
    .dev_data = (void *) DEV_ZERO,
    .write = pseudo_write,
    .read = pseudo_read
};
static struct dev_entry _ent_zero = {
    .dev = &_dev_zero,
    .dev_class = DEV_CLASS_BLOCK,
    .dev_subclass = DEV_BLOCK_PSEUDO,
    .dev_name = "zero"
};

static ssize_t pseudo_write(struct blkdev *dev, const void *buf, size_t pos, size_t lim) {
    switch ((uint64_t) dev->dev_data) {
    case DEV_NULL:
    case DEV_ZERO:
        return lim;
    default:
        panic("Unexpected dev id: %p\n", dev->dev_data);
    }
}

static ssize_t pseudo_read(struct blkdev *dev, void *buf, size_t pos, size_t lim) {
    switch ((uint64_t) dev->dev_data) {
    case DEV_ZERO:
        memset(buf, 0, lim);
        return lim;
    case DEV_NULL:
        return 0;
    default:
        panic("Unexpected dev id: %p\n", dev->dev_data);
    }
}

static __init void pseudo_init(void) {
    dev_entry_add(&_ent_null);
    dev_entry_add(&_ent_zero);
}
