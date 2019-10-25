#include "sys/blk/ram.h"
#include "sys/string.h"

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

static struct blkdev _ramblk0 = {
    .dev_data = &ram_priv,
    .read = ramblk_read,
    .write = NULL
};
struct blkdev *ramblk0 = &_ramblk0;

void ramblk_init(uintptr_t at, size_t len) {
    ram_priv.begin = at;
    ram_priv.lim = len;
}
