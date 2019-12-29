#include "sys/dev.h"
#include "sys/blk.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/string.h"

static struct dev_entry *dev_begin = NULL;
static struct dev_entry *dev_end = NULL;

static uint64_t dev_scsi_bitmap = 0;
static uint64_t dev_ide_bitmap = 0;
static int dev_ram_count = 0;

static int dev_alloc_block_name(uint16_t subclass, char *name) {
    if (subclass == DEV_BLOCK_RAM) {
        strcpy(name, "ram");
        name[3] = dev_ram_count++ + '0';
        name[4] = 0;
        return 0;
    }
    if (subclass == DEV_BLOCK_SDx || subclass == DEV_BLOCK_HDx) {
        uint64_t *bmp = subclass == DEV_BLOCK_SDx ? &dev_scsi_bitmap : &dev_ide_bitmap;
        name[1] = 'd';
        name[0] = subclass == DEV_BLOCK_SDx ? 's' : 'h';

        for (size_t i = 0; i < 64; ++i) {
            if (!(*bmp & (1ULL << i))) {
                *bmp |= 1ULL << i;
                name[2] = 'a' + i;
                name[3] = 0;
                return 0;
            }
        }
    }
    return -1;
}

int dev_alloc_name(enum dev_class cls, uint16_t subclass, char *name) {
    switch (cls) {
    case DEV_CLASS_BLOCK:
        return dev_alloc_block_name(subclass, name);
    default:
        panic("Not implemented\n");
    }
}

static int dev_post_add(struct dev_entry *ent) {
    if (ent->dev_class == DEV_CLASS_BLOCK) {
        struct blkdev *blk = (struct blkdev *) ent->dev;

        blk->ent = ent;

        if (ent->dev_subclass < DEV_BLOCK_PART) {
            if (blk_enumerate_partitions((struct blkdev *) ent->dev) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

void dev_entry_add(struct dev_entry *ent) {
    ent->cdr = NULL;

    if (dev_end) {
        dev_end->cdr = ent;
    } else {
        dev_begin = ent;
    }

    dev_end = ent;

    kdebug("Add device %s\n", ent->dev_name);

    // For example: enumerate partitions on block devices
    _assert(dev_post_add(ent) == 0);
}

struct dev_entry *dev_iter(void) {
    return dev_begin;
}
