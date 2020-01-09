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

const char GUID_EMPTY[16] = {0};

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

struct gpt_header {
    char signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t __res0;
    uint64_t my_lba;
    uint64_t alt_lba;
    uint64_t first_lba;
    uint64_t last_lba;
    char disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t partition_count;
    uint32_t partition_entry_size;
    uint32_t partition_array_crc32;
} __attribute__((packed));

struct gpt_part_entry {
    char type_guid[16];
    char part_guid[16];
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t attr;
    char part_name[72];
};

struct blk_part {
    struct blkdev *device;
    uint64_t lba_start;
    uint64_t lba_size;
};

int blk_mount_auto(struct vfs_node *at, struct blkdev *blk, const char *opt) {
    int res;

    if ((res = vfs_mount_internal(at, blk, "ext2", opt)) == 0) {
        return 0;
    }

    return -1;
}

static ssize_t blk_part_write(struct blkdev *dev, const void *buf, size_t off, size_t count) {
    struct blk_part *part = (struct blk_part *) dev->dev_data;
    size_t part_size_bytes = part->lba_size * 512;
    size_t part_off_bytes = part->lba_start * 512;

    if (off >= part_size_bytes) {
        return -1;
    }

    size_t l = MIN(part_size_bytes - off, count);

    return blk_write(part->device, buf, off + part_off_bytes, l);
}

static ssize_t blk_part_read(struct blkdev *dev, void *buf, size_t off, size_t count) {
    struct blk_part *part = (struct blk_part *) dev->dev_data;
    size_t part_size_bytes = part->lba_size * 512;
    size_t part_off_bytes = part->lba_start * 512;

    if (off >= part_size_bytes) {
        return -1;
    }

    size_t l = MIN(part_size_bytes - off, count);

    return blk_read(part->device, buf, off + part_off_bytes, l);
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
            blk->write = blk_part_write;
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

static int blk_enumerate_gpt(struct blkdev *dev, uint8_t *head) {
    // LBA1 - GPT header
    struct gpt_header *hdr = (struct gpt_header *) &head[512];
    // Read a single block for partition table entries
    char buf[512];

    _assert(blk_read(dev, buf, hdr->partition_entry_lba * 512, 512) >= 0);

    size_t offset = 0;
    size_t part_n = 0;
    for (size_t i = 0; i < hdr->partition_count; ++i) {
        struct gpt_part_entry *ent = (struct gpt_part_entry *) &buf[offset];

        if (strncmp(ent->type_guid, GUID_EMPTY, 16)) {
            // Maybe some known filesystem here, ignore partition type guid

            struct blk_part *part = (struct blk_part *) kmalloc(sizeof(struct blk_part));
            part->device = dev;
            part->lba_start = ent->start_lba;
            part->lba_size = ent->end_lba - ent->start_lba;

            void *buf = kmalloc(sizeof(struct blkdev) + sizeof(struct dev_entry));
            struct dev_entry *ent = (struct dev_entry *) buf;
            struct blkdev *blk = (struct blkdev *) ((uintptr_t) buf + sizeof(struct dev_entry));
            _assert(ent);

            blk->dev_data = part;
            blk->read = blk_part_read;
            blk->write = blk_part_write;
            blk->ent_parent = dev->ent;

            ent->dev = blk;
            ent->dev_class = DEV_CLASS_BLOCK;
            ent->dev_subclass = DEV_BLOCK_PART;
            // XXX: This is ugly
            strcpy(ent->dev_name, dev->ent->dev_name);
            char digit[2] = { '1' + part_n++, 0 };
            strcat(ent->dev_name, digit);

            dev_entry_add(ent);
        }

        offset += hdr->partition_entry_size;
        if (offset >= 512) {
            break;
        }
    }

    return 0;
}

struct blkdev *blk_by_name(const char *name) {
    struct dev_entry *it = dev_iter();

    for (; it; it = it->cdr) {
        if (!strcmp(it->dev_name, name)) {
            return (struct blkdev *) it->dev;
        }
    }

    return NULL;
}

int blk_enumerate_partitions(struct blkdev *dev) {
    kdebug("Enumerating partitions of %s\n", dev->ent->dev_name);
    // TODO: handle stuff where block size is not 512
    uint8_t buf[1024];

    _assert(blk_read(dev, buf, 0, 1024) >= 0);

    if (!strncmp((const char *) (buf + 512), "EFI PART", 8)) {
        // Found GPT
        return blk_enumerate_gpt(dev, buf);
    } else if (buf[510] == 0x55 && buf[511] == 0xAA) {
        // Found MBR
        return blk_enumerate_mbr(dev, buf);
    }

    // No partition table - just a plain block device
    return 0;
}
