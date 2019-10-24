#include "sys/blk.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/string.h"
#include "sys/heap.h"

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


// TODO: guess this would better be in blk_part.c or something like that
struct mbr {
    char bootstrap[440];
    uint32_t uuid;
    uint16_t opt;
    struct mbr_entry {
        uint8_t attr;
        uint8_t chs_start[3];
        uint8_t type;
        uint8_t chs_end[3];
        uint32_t lba_start;
        uint32_t nsectors;
    } __attribute__((packed)) entries[4];
    uint16_t sign;
} __attribute__((packed));

struct blk_part {
    struct blkdev *device;
    uint64_t lba_start;
    uint64_t lba_size;
};

static ssize_t blk_part_read(struct blkdev *dev, void *buf, size_t off, size_t count) {
    return -1;
}

static int blk_enumerate_mbr(struct blkdev *dev, uint8_t *head) {
    kdebug("Enumerating MBR partitions\n");
    struct mbr *mbr = (struct mbr *) head;

    for (size_t i = 0; i < 4; ++i) {
        if (mbr->entries[i].chs_start[1] && mbr->entries[i].nsectors) {
            kdebug("Partition %u: Start %u, End %u, %u Sectors (%S), Type 0x%02x\n",
                    i,
                    mbr->entries[i].lba_start,
                    mbr->entries[i].lba_start + mbr->entries[i].nsectors,
                    mbr->entries[i].nsectors,
                    mbr->entries[i].nsectors * 512,
                    mbr->entries[i].type);

            struct blk_part *part = (struct blk_part *) kmalloc(sizeof(struct blk_part));
            part->device = dev;
            part->lba_start = mbr->entries[i].lba_start;
            part->lba_size = mbr->entries[i].nsectors * 512;

            void *buf = kmalloc(sizeof(struct blkdev) + sizeof(struct dev_entry));
            struct dev_entry *ent = (struct dev_entry *) buf;
            struct blkdev *blk = (struct blkdev *) ((uintptr_t) buf + sizeof(struct dev_entry));
            _assert(ent);

            blk->dev_data = part;
            blk->read = blk_part_read;
            blk->ent_parent = dev->ent;

            ent->dev = blk;
            ent->dev_class = DEV_CLASS_BLOCK;
            ent->dev_subclass = DEV_BLOCK_PART;
            // XXX: This is ugly
            strcpy(ent->dev_name, dev->ent->dev_name);
            char digit[2] = { '1' + i, 0 };
            strcat(ent->dev_name, digit);

            dev_entry_add(ent);
        }
    }

    return 0;
}

int blk_enumerate_partitions(struct blkdev *dev) {
    kdebug("Enumerating partitions of %s\n", dev->ent->dev_name);
    uint8_t buf[1024];

    _assert(blk_read(dev, buf, 0, 1024) >= 0);

    if (!strncmp((const char *) (buf + 512), "EFI PART", 8)) {
        // Found GPT
        panic("Found GPT: not supported\n");
    } else if (buf[510] == 0x55 && buf[511] == 0xAA) {
        // Found MBR
        return blk_enumerate_mbr(dev, buf);
    }

    // No partition table - just a plain block device
    return 0;
}
