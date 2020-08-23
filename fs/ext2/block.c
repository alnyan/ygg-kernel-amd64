#include "fs/ext2/block.h"
#include "fs/ext2/ext2.h"
#include "user/errno.h"
#include "sys/block/blk.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "fs/ext2/alloc.h"
#include "fs/fs.h"
#include "sys/debug.h"

int ext2_read_superblock(struct fs *fs) {
    struct ext2_data *data = fs->fs_private;
    _assert(data);
    int res = blk_read(fs->blk, data->__data, 1024, 1024);
    if (res == 1024) {
        return 0;
    }
    kerror("Superblock read failed\n");
    return res;
}

int ext2_write_superblock(struct fs *fs) {
    struct ext2_data *data = fs->fs_private;
    _assert(data);
    int res = blk_write(fs->blk, data->__data, 1024, 1024);
    if (res == 1024) {
        return 0;
    }
    kerror("Superblock write failed\n");
    return res;
}

int ext2_read_block(struct fs *fs, void *block, uint32_t no) {
    struct ext2_data *data = fs->fs_private;
    _assert(data);
    int res = blk_read(fs->blk, block, no * data->block_size, data->block_size);
    if (res == 1024) {
        return 0;
    }
    kerror("Failed to read block %u: %s\n", no, kstrerror(res));
    return res;
}

int ext2_write_block(struct fs *fs, const void *block, uint32_t no) {
    struct ext2_data *data = fs->fs_private;
    _assert(data);
    int res = blk_write(fs->blk, block, no * data->block_size, data->block_size);
    if (res == 1024) {
        return 0;
    }
    kerror("Failed to write block %u: %s\n", no, kstrerror(res));
    return res;
}

int ext2_read_inode(struct fs *fs, struct ext2_inode *inode, uint32_t ino) {
    struct ext2_data *data = fs->fs_private;
    _assert(data);
    char block[data->block_size];
    if (ino < 1 || ino >= data->sb.inode_count) {
        panic("Invalid inode number: %u\n", ino);
    }
    int res;

    --ino;
    uint32_t ino_group = ino / data->sb.block_group_inodes;
    uint32_t ino_in_group = ino % data->sb.block_group_inodes;
    uint32_t ino_block = data->bgdt[ino_group].inode_table_no + ino_in_group / data->inodes_per_block;
    uint32_t offset_in_block = (ino_in_group % data->inodes_per_block) * data->inode_size;
    _assert(offset_in_block < data->block_size);

    if ((res = ext2_read_block(fs, block, ino_block)) != 0) {
        return res;
    }

    memcpy(inode, offset_in_block + block, data->inode_size);

    return 0;
}

int ext2_write_inode(struct fs *fs, struct ext2_inode *inode, uint32_t ino) {
    // Automatically update inode times
    inode->atime = time();

    struct ext2_data *data = fs->fs_private;
    _assert(data);
    char block[data->block_size];
    if (ino < 1 || ino >= data->sb.inode_count) {
        panic("Invalid inode number: %u\n", ino);
    }
    int res;

    --ino;
    uint32_t ino_group = ino / data->sb.block_group_inodes;
    uint32_t ino_in_group = ino % data->sb.block_group_inodes;
    uint32_t ino_block = data->bgdt[ino_group].inode_table_no + ino_in_group / data->inodes_per_block;
    uint32_t offset_in_block = (ino_in_group % data->inodes_per_block) * data->inode_size;
    _assert(offset_in_block < data->block_size);

    if ((res = ext2_read_block(fs, block, ino_block)) != 0) {
        return res;
    }

    memcpy(offset_in_block + block, inode, data->inode_size);

    return ext2_write_block(fs, block, ino_block);
}

int ext2_read_inode_block(struct fs *fs, struct ext2_inode *inode, void *buf, uint32_t index) {
    uint32_t block = ext2_inode_get_index(fs, inode, index);
    if (!block) {
        panic("Read outside of block count range\n");
    }
    return ext2_read_block(fs, buf, block);
}

int ext2_write_inode_block(struct fs *fs, struct ext2_inode *inode, const void *buf, uint32_t index) {
    uint32_t block = ext2_inode_get_index(fs, inode, index);
    if (!block) {
        panic("Write outside of block count range\n");
    }
    return ext2_write_block(fs, buf, block);
}

uint32_t ext2_inode_get_index(struct fs *ext2, struct ext2_inode *inode, uint32_t index) {
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    uint32_t p = data->block_size / sizeof(uint32_t);
    uint32_t ptrs[p];

    if (index < EXT2_DIRECT_BLOCKS) {
        return inode->direct_blocks[index];
    }

    // Fits in L1?
    index -= EXT2_DIRECT_BLOCKS;
    if (index < p) {
        if (!inode->indirect_block_l1) {
            panic("Read beyond end of file (L1.1)\n");
        }

        _assert(ext2_read_block(ext2, ptrs, inode->indirect_block_l1) == 0);
        return ptrs[index];
    }

    // Fits in L2?
    index -= p;
    if (index < p * p) {
        uint32_t index_l1 = index / p;
        uint32_t index_l0 = index % p;
        if (!inode->indirect_block_l2) {
            panic("Read beyond end of the file (L2.2)\n");
        }
        _assert(ext2_read_block(ext2, ptrs, inode->indirect_block_l2) == 0);
        if (!ptrs[index_l1]) {
            panic("Read beyond end of the file (L2.1)\n");
        }
        _assert(ext2_read_block(ext2, ptrs, ptrs[index_l1]) == 0);
        return ptrs[index_l0];
    }

    panic("TODO: L3 support\n");
}

int ext2_inode_set_index(struct fs *ext2, struct ext2_inode *inode, uint32_t ino, uint32_t index, uint32_t value) {
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    uint32_t p = data->block_size / sizeof(uint32_t);
    uint32_t a, b, c, d, e, f, g;
    uint32_t ptrs[p];

    if (index < EXT2_DIRECT_BLOCKS) {
        inode->direct_blocks[index] = value;
        return 0;
    } else if (index < EXT2_DIRECT_BLOCKS + p) {
        if (!inode->indirect_block_l1) {
            uint32_t block = ext2_alloc_block(ext2, data);
            if (!block) {
                panic("Failed to allocate a block\n");
            }
            inode->indirect_block_l1 = block;
        }

        _assert(ext2_read_block(ext2, ptrs, inode->indirect_block_l1) == 0);
        ptrs[index - EXT2_DIRECT_BLOCKS] = value;
        _assert(ext2_write_block(ext2, ptrs, inode->indirect_block_l1) == 0);
        return 0;
    } else {
        panic("TODO L2+\n");
    }
}

