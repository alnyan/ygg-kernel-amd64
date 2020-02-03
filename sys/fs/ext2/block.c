#include "sys/fs/ext2/block.h"
#include "sys/fs/ext2/ext2.h"
#include "sys/user/errno.h"
#include "sys/block/blk.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/fs/fs.h"
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

int ext2_read_block(struct fs *fs, void *block, uint32_t no) {
    struct ext2_data *data = fs->fs_private;
    _assert(data);
    kdebug("Read block %u: %p\n", no, no * data->block_size);
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
    kdebug("Write block %u: %p\n", no, no * data->block_size);
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
    inode->mtime = time();
    inode->atime = inode->mtime;

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
    if (index < 12) {
        return ext2_read_block(fs, buf, inode->direct_blocks[index]);
    } else {
        panic("Not implemented\n");
    }
}
