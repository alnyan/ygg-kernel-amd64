#include "sys/blk.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/string.h"
#include "sys/heap.h"
#include "sys/fs/vfs.h"
#include "sys/fs/fs.h"

// Taken as-is from MBR partition types
#define PART_HINT_LINUX              0x83

const char GUID_EMPTY[16] = {0};

struct blk_part {
    struct blkdev *device;
    uint64_t lba_start;
    uint64_t size;
};

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

void *blk_mmap(struct blkdev *blk, struct ofile *of, void *hint, size_t length, int flags) {
    _assert(blk);
    if (blk->mmap) {
        return blk->mmap(blk, of, hint, length, flags);
    } else {
        return (void *) -EINVAL;
    }
}

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

static int blk_add_part(struct vnode *of, int n, uint64_t lba, uint64_t size) {
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

static int blk_enumerate_gpt(struct blkdev *dev, struct vnode *of, uint8_t *head) {
    // LBA1 - GPT header
    struct gpt_header *hdr = (struct gpt_header *) &head[512];
    // Read a single block for partition table entries
    char buf[512];

    _assert(blk_read(dev, buf, hdr->partition_entry_lba * 512, 512) >= 0);

    size_t offset = 0;
    size_t part_n = 0;
    int res;

    for (size_t i = 0; i < hdr->partition_count; ++i) {
        struct gpt_part_entry *ent = (struct gpt_part_entry *) &buf[offset];

        if (strncmp(ent->type_guid, GUID_EMPTY, 16)) {
            // Maybe some known filesystem here, ignore partition type guid
            if ((res = blk_add_part(of, i, ent->start_lba, ent->end_lba - ent->start_lba)) != 0) {
                kdebug("Failed to add partition: %s\n", kstrerror(res));
            }
        }

        offset += hdr->partition_entry_size;
        if (offset >= 512) {
            break;
        }
    }

    return 0;
}

int blk_enumerate_partitions(struct blkdev *blk, struct vnode *node) {
    uint8_t buf[1024];
    ssize_t res;

    if ((res = blk_read(blk, buf, 0, 1024)) < 0) {
        return res;
    }


    if (!strncmp((const char *) (buf + 512), "EFI PART", 8)) {
        return blk_enumerate_gpt(blk, node, buf);
    }

    return 0;
}
