#include "sys/blk.h"
#include "sys/fs/node.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/errno.h"
#include "sys/debug.h"

// Align to some nice boundary (it's 36)
#define GUID_STRLEN             40

const char GUID_EMPTY[16] = {0};
const struct {
    const char *name;
    const char *guid;
} guiddb[] = {
    // A bunch of known GUID so I can pretty-print stuff
    { "EFI System Partition",               "C12A7328-F81F-11D2-BA4B-00A0C93EC93B" },
    { "Linux Filesystem",                   "0FC63DAF-8483-4772-8E79-3D69D8477DE4" },
    { "Microsoft Basic Data",               "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7" },
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

static void strguid(char *str, const char *guid) {
    static const char *const hexc = "0123456789ABCDEF";
    // TODO: snprintf
    int off = 0;
    for (int i = 3; i >= 0; --i) {
        str[off++] = hexc[(guid[i] >> 4) & 0xF];
        str[off++] = hexc[guid[i] & 0xF];
    }
    str[off++] = '-';
    for (int i = 1; i >= 0; --i) {
        str[off++] = hexc[(guid[i + 4] >> 4) & 0xF];
        str[off++] = hexc[guid[i + 4] & 0xF];
    }
    str[off++] = '-';
    for (int i = 1; i >= 0; --i) {
        str[off++] = hexc[(guid[i + 6] >> 4) & 0xF];
        str[off++] = hexc[guid[i + 6] & 0xF];
    }
    str[off++] = '-';
    for (int i = 0; i < 2; ++i) {
        str[off++] = hexc[(guid[i + 8] >> 4) & 0xF];
        str[off++] = hexc[guid[i + 8] & 0xF];
    }
    str[off++] = '-';
    for (int i = 0; i < 6; ++i) {
        str[off++] = hexc[(guid[i + 10] >> 4) & 0xF];
        str[off++] = hexc[guid[i + 10] & 0xF];
    }
    str[off] = 0;
}

static void gpt_describe_entry(struct vnode *of, size_t i, struct gpt_part_entry *ent) {
    char type_guid_str[GUID_STRLEN];
    char part_guid_str[GUID_STRLEN];
    kdebug("%s partition %d:\n", of->name, i);
    strguid(type_guid_str, ent->type_guid);
    strguid(part_guid_str, ent->part_guid);
    kdebug("Type: %s\n", type_guid_str);
    for (size_t j = 0; j < sizeof(guiddb) / sizeof(guiddb[0]); ++j) {
        if (!strncmp(guiddb[j].guid, type_guid_str, GUID_STRLEN)) {
            kdebug("  %s\n", guiddb[j].name);
            break;
        }
    }
    kdebug("Part: %s\n", part_guid_str);
}

int blk_enumerate_gpt(struct blkdev *dev, struct vnode *of, uint8_t *head) {
    // TODO: support other devices
    _assert(dev->block_size == 512);
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

        if (memcmp(ent->type_guid, GUID_EMPTY, 16)) {
            // Maybe some known filesystem here, ignore partition type guid
            if ((res = blk_add_part(of, i, ent->start_lba, ent->end_lba - ent->start_lba)) != 0) {
                kerror("Failed to add partition: %s\n", kstrerror(res));
            } else {
                gpt_describe_entry(of, i, ent);
            }
        }

        offset += hdr->partition_entry_size;
        if (offset >= 512) {
            break;
        }
    }

    return 0;
}

