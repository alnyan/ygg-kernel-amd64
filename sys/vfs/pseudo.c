#include "sys/string.h"
#include "sys/random.h"
#include "sys/panic.h"
#include "sys/errno.h"
#include "sys/attr.h"
#include "sys/blk.h"
#include "sys/dev.h"

#define DEV_ZERO        0
#define DEV_NULL        1
#define DEV_URANDOM     2

static ssize_t pseudo_write(struct blkdev *dev, const void *buf, size_t pos, size_t lim);
static ssize_t pseudo_read(struct blkdev *dev, void *buf, size_t pos, size_t lim);

static void urandom_fill(void *buf, size_t lim) {
    uint64_t val;
    size_t i = 0;

    while (i < lim) {
        val = random_single();

        memcpy(buf + i, &val, MIN(lim - i, 8));

        i += 8;
    }
}

static struct blkdev _dev_null = {
    .dev_data = (void *) DEV_NULL,
    .write = pseudo_write,
    .read = pseudo_read
};
static struct blkdev _dev_zero = {
    .dev_data = (void *) DEV_ZERO,
    .write = pseudo_write,
    .read = pseudo_read
};
static struct blkdev _dev_urandom = {
    .dev_data = (void *) DEV_URANDOM,
    .write = pseudo_write,
    .read = pseudo_read
};

static ssize_t pseudo_write(struct blkdev *dev, const void *buf, size_t pos, size_t lim) {
    switch ((uint64_t) dev->dev_data) {
    case DEV_NULL:
    case DEV_ZERO:
        return lim;
    case DEV_URANDOM:
        return -EINVAL;
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
    case DEV_URANDOM:
        urandom_fill(buf, lim);
        return lim;
    default:
        panic("Unexpected dev id: %p\n", dev->dev_data);
    }
}

// XXX: This is called before heap init, fucked up?
void pseudo_init(void) {
    // FIXME: They're actually character devices
    dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_PSEUDO, &_dev_null, "null");
    dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_PSEUDO, &_dev_zero, "zero");
    dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_PSEUDO, &_dev_urandom, "urandom");
}
