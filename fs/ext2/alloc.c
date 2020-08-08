#include "fs/ext2/alloc.h"
#include "fs/ext2/block.h"
#include "fs/ext2/ext2.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "fs/fs.h"

uint32_t ext2_alloc_inode(struct fs *fs, struct ext2_data *data, int is_dir) {
    union {
        char bytes[data->block_size];
        uint64_t bitmap[data->block_size / sizeof(uint64_t)];
    } buf;
    struct ext2_blkgrp_desc *bgd;
    uint32_t ino = 0, offset;
    uint32_t group = 0;

    for (uint32_t i = 0; i < data->bgdt_entry_count; ++i) {
        bgd = &data->bgdt[i];
        if (!bgd->free_inodes) {
            continue;
        }

        ext2_read_block(fs, buf.bytes, bgd->inode_bitmap_no);

        for (size_t j = 0; j < data->sb.block_group_inodes; ++j) {
            // "j" is block number inside the "i" group
            if (!(buf.bitmap[j / 64] & (1ULL << (j % 64)))) {
                ino = i * data->sb.block_group_inodes + j + 1;
                group = i;
                offset = j;
                break;
            }
        }
    }

    if (!ino) {
        return 0;
    }

    kdebug("Allocated inode: %d\n", ino);

    bgd = &data->bgdt[group];
    buf.bitmap[offset / 64] |= 1ULL << (offset % 64);
    if (ext2_write_block(fs, buf.bytes, bgd->inode_bitmap_no) != 0) {
        panic("Failed to write BGDT bitmap block back\n");
    }

    // Write modified BGD back
    --bgd->free_inodes;
    if (is_dir) {
        ++bgd->directory_count;
    }
    size_t bgd_block_number = (group * sizeof(struct ext2_blkgrp_desc)) / data->block_size;
    if (ext2_write_block(fs, ((void *) data->bgdt) + bgd_block_number * data->block_size, bgd_block_number + 2) != 0) {
        panic("EEEE\n");
    }

    --data->sb.free_inodes;
    if (ext2_write_superblock(fs) != 0) {
        panic("REEE\n");
    }

    return ino;
}

// TODO: what if BGD has more than single-block block bitmap
uint32_t ext2_alloc_block(struct fs *fs, struct ext2_data *data) {
    union {
        char bytes[data->block_size];
        uint64_t bitmap[data->block_size / sizeof(uint64_t)];
    } buf;
    struct ext2_blkgrp_desc *bgd;
    uint32_t block = 0, offset;
    uint32_t group = 0;

    for (uint32_t i = 0; i < data->bgdt_entry_count; ++i) {
        // "i" is a block group index
        bgd = &data->bgdt[i];

        if (!bgd->free_blocks) {
            continue;
        }

        ext2_read_block(fs, buf.bytes, bgd->block_bitmap_no);

        for (size_t j = 0; j < data->sb.block_group_blocks; ++j) {
            // "j" is block number inside the "i" group
            if (!(buf.bitmap[j / 64] & (1ULL << (j % 64)))) {
                block = i * data->sb.block_group_blocks + j + 1;
                group = i;
                offset = j;
                break;
            }
        }
    }

    if (!block) {
        panic("Failed to allocate a block\n");
    }

    // Write modified bitmap block back
    bgd = &data->bgdt[group];
    buf.bitmap[offset / 64] |= 1ULL << (offset % 64);
    if (ext2_write_block(fs, buf.bytes, bgd->block_bitmap_no) != 0) {
        panic("Failed to write BGDT bitmap block back\n");
    }

    kdebug("Alloc block %d\n", block);

    // Write modified BGD back
    --bgd->free_blocks;
    size_t bgd_block_number = (group * sizeof(struct ext2_blkgrp_desc)) / data->block_size;
    if (ext2_write_block(fs, ((void *) data->bgdt) + bgd_block_number * data->block_size, bgd_block_number + 2) != 0) {
        panic("EEEE\n");
    }

    --data->sb.free_blocks;
    if (ext2_write_superblock(fs) != 0) {
        panic("REEE\n");
    }

    return block;
}

void ext2_free_inode(struct fs *fs, struct ext2_data *data, uint32_t ino, int is_dir) {
    struct ext2_blkgrp_desc *bgd;
    union {
        char bytes[data->block_size];
        uint64_t bitmap[data->block_size / sizeof(uint64_t)];
    } buf;
    if (!ino) {
        panic("Invalid inode number\n");
    }

    --ino;
    uint32_t offset = ino % data->sb.block_group_inodes;
    uint32_t group = ino / data->sb.block_group_inodes;

    bgd = &data->bgdt[group];
    // Read bitmap
    if (ext2_read_block(fs, buf.bytes, bgd->inode_bitmap_no) != 0) {
        panic("Failed to read inode bitmap\n");
    }

    buf.bitmap[offset / 64] &= ~(1ULL << (offset % 64));

    // Write bitmap back
    if (ext2_write_block(fs, buf.bytes, bgd->inode_bitmap_no) != 0) {
        panic("Failed to write inode bitmap\n");
    }

    // Write modified BGD back
    ++bgd->free_inodes;
    if (is_dir) {
        --bgd->directory_count;
    }
    size_t bgd_block_number = (group * sizeof(struct ext2_blkgrp_desc)) / data->block_size;
    if (ext2_write_block(fs, ((void *) data->bgdt) + bgd_block_number * data->block_size, bgd_block_number + 2) != 0) {
        panic("EEEE\n");
    }

    ++data->sb.free_inodes;
    if (ext2_write_superblock(fs) != 0) {
        panic("REEE\n");
    }
}

void ext2_free_block(struct fs *fs, struct ext2_data *data, uint32_t blk) {
    struct ext2_blkgrp_desc *bgd;
    union {
        char bytes[data->block_size];
        uint64_t bitmap[data->block_size / sizeof(uint64_t)];
    } buf;
    if (!blk) {
        panic("Invalid block number\n");
    }

    --blk;
    uint32_t offset = blk % data->sb.block_group_blocks;
    uint32_t group = blk / data->sb.block_group_blocks;

    bgd = &data->bgdt[group];
    // Read bitmap
    if (ext2_read_block(fs, buf.bytes, bgd->block_bitmap_no) != 0) {
        panic("Failed to read block bitmap\n");
    }

    buf.bitmap[offset / 64] &= ~(1ULL << (offset % 64));

    // Write bitmap back
    if (ext2_write_block(fs, buf.bytes, bgd->block_bitmap_no) != 0) {
        panic("Failed to write block bitmap\n");
    }

    // Write modified BGD back
    ++bgd->free_blocks;
    size_t bgd_block_number = (group * sizeof(struct ext2_blkgrp_desc)) / data->block_size;
    if (ext2_write_block(fs, ((void *) data->bgdt) + bgd_block_number * data->block_size, bgd_block_number + 2) != 0) {
        panic("EEEE\n");
    }

    ++data->sb.free_blocks;
    if (ext2_write_superblock(fs) != 0) {
        panic("REEE\n");
    }
}

int ext2_file_resize(struct fs *ext2, uint32_t ino, struct ext2_inode *inode, size_t new_size) {
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    size_t block_count = (new_size + data->block_size - 1) / data->block_size;
    size_t old_block_count = (inode->size_lower + data->block_size - 1) / data->block_size;

    uint32_t p = data->block_size / sizeof(uint32_t);

    // TODO: handle L2/L3
    _assert(block_count <= EXT2_DIRECT_BLOCKS + p && old_block_count <= EXT2_DIRECT_BLOCKS + p);

    if (block_count >= old_block_count) {
        for (size_t index = old_block_count; index < block_count; ++index) {
            uint32_t block = ext2_alloc_block(ext2, data);
            if (!block) {
                panic("Failed to allocate a new block\n");
            }

            _assert(ext2_inode_set_index(ext2, inode, ino, index, block) == 0);
        }
    } else {
        // (Depth-first) free data blocks
        for (size_t index = block_count; index < old_block_count; ++index) {
            uint32_t block = ext2_inode_get_index(ext2, inode, index);
            _assert(block);
            ext2_inode_set_index(ext2, inode, ino, index, 0);
            ext2_free_block(ext2, data, block);
        }

        if (old_block_count > EXT2_DIRECT_BLOCKS && block_count <= EXT2_DIRECT_BLOCKS + p) {
            // Free L1 pointer block
            _assert(inode->indirect_block_l1);
            ext2_free_block(ext2, data, inode->indirect_block_l1);
            inode->indirect_block_l1 = 0;
        }

        if (old_block_count > EXT2_DIRECT_BLOCKS + p && block_count <= EXT2_DIRECT_BLOCKS + p + p * p) {
            // Free L2 pointer block
            panic("TODO: L2 support\n");
        }
    }

    inode->size_lower = new_size;
    size_t real_block_count = block_count;
    if (block_count > EXT2_DIRECT_BLOCKS) {
        ++real_block_count;
    }
    inode->sector_count = (real_block_count * data->block_size + 511) / 512;

    return 0;
}
