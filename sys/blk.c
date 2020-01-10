#include "sys/blk.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/string.h"
#include "sys/heap.h"
#include "sys/fs/vfs.h"

// Taken as-is from MBR partition types
#define PART_HINT_LINUX              0x83

ssize_t blk_read(struct blkdev *blk, void *buf, size_t off, size_t lim) {
    _assert(blk);

    if (blk->read) {
        return blk->read(blk, buf, off, lim);
    } else {
        return -EINVAL;
    }
}

ssize_t blk_write(struct blkdev *blk, const void *buf, size_t off, size_t lim) {
    _assert(blk);

    if (blk->write) {
        return blk->write(blk, buf, off, lim);
    } else {
        return -EINVAL;
    }
}
