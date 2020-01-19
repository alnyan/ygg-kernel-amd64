#include "sys/fs/ext2.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/errno.h"
#include "sys/debug.h"

#define ext2_super(e)       ((struct ext2_info *) (e)->fs_private)

int ext2_write_superblock(struct fs *ext2) {
    struct ext2_info *info = ext2->fs_private;
    return blk_write(ext2->blk, info, EXT2_SBOFF, EXT2_SBSIZ);
}

int ext2_read_block(struct fs *ext2, uint32_t block_no, void *buf) {
    if (!block_no) {
        return -1;
    }
    //printf("ext2_read_block %u\n", block_no);
    int res = blk_read(ext2->blk, buf, block_no * ext2_super(ext2)->block_size, ext2_super(ext2)->block_size);

    if (res < 0) {
        //fprintf(stderr, "ext2: Failed to read %uth block\n", block_no);
        kerror("ext2: Failed to read %uth block\n", block_no);
    }

    return res;
}

int ext2_write_block(struct fs *ext2, uint32_t block_no, const void *buf) {
    if (!block_no) {
        return -1;
    }

    int res = blk_write(ext2->blk, buf, block_no * ext2_super(ext2)->block_size, ext2_super(ext2)->block_size);

    if (res < 0) {
        //fprintf(stderr, "ext2: Failed to write %uth block\n", block_no);
        kerror("ext2: Failed to write %uth block\n", block_no);
    }

    return res;
}

static uint32_t ext2_get_inode_block(struct fs *ext2, struct ext2_inode *inode, uint32_t index) {
    if (index < 12) {
        return inode->direct_blocks[index];
    } else {
        struct ext2_info *info = ext2->fs_private;
        char buf[1024];
        int res;

        if (index < 12 + (info->block_size / 4)) {
            if ((res = ext2_read_block(ext2, inode->l1_indirect_block, buf)) < 0) {
                return res;
            }

            return ((uint32_t *) buf)[index - 12];
        } else {
            panic("Inode block index has too high indirection level\n");
        }
    }
}

int ext2_write_inode_block(struct fs *ext2, struct ext2_inode *inode, uint32_t index, const void *buf) {
    uint32_t block_number = ext2_get_inode_block(ext2, inode, index);
    return ext2_write_block(ext2, block_number, buf);
}

int ext2_read_inode_block(struct fs *ext2, struct ext2_inode *inode, uint32_t index, void *buf) {
    uint32_t block_number = ext2_get_inode_block(ext2, inode, index);
    return ext2_read_block(ext2, block_number, buf);
}

int ext2_read_inode(struct fs *ext2, struct ext2_inode *inode, uint32_t ino) {
    struct ext2_info *info = ext2->fs_private;
    //printf("ext2_read_inode %d\n", ino);
    char inode_block_buffer[info->block_size];

    uint32_t ino_block_group_number = (ino - 1) / info->sb.block_group_size_inodes;
    //printf("inode block group number = %d\n", ino_block_group_number);
    uint32_t ino_inode_table_block = info->block_group_descriptor_table[ino_block_group_number].inode_table_block;
    //printf("inode table is at block %d\n", ino_inode_table_block);
    uint32_t ino_inode_index_in_group = (ino - 1) % info->sb.block_group_size_inodes;
    //printf("inode entry index in the group = %d\n", ino_inode_index_in_group);
    uint32_t ino_inode_block_in_group = (ino_inode_index_in_group * info->sb.inode_struct_size) / info->block_size;
    //printf("inode entry offset is %d blocks\n", ino_inode_block_in_group);
    uint32_t ino_inode_block_number = ino_inode_block_in_group + ino_inode_table_block;
    //printf("inode block number is %uth block\n", ino_inode_block_number);

    //struct ext2_inode *root_inode_block_inode_table = (struct ext2_inode *) root_inode_block_buf;
    if (ext2_read_block(ext2, ino_inode_block_number, inode_block_buffer) < 0) {
        kerror("ext2: failed to load inode#%d block\n", ino);
        return -1;
    }

    uint32_t ino_entry_in_block = (ino_inode_index_in_group * info->sb.inode_struct_size) % info->block_size;
    memcpy(inode, &inode_block_buffer[ino_entry_in_block], info->sb.inode_struct_size);

    return 0;
}

int ext2_write_inode(struct fs *ext2, const struct ext2_inode *inode, uint32_t ino) {
    struct ext2_info *info = ext2->fs_private;
    //printf("ext2_read_inode %d\n", ino);
    char inode_block_buffer[info->block_size];
    int res;

    uint32_t ino_block_group_number = (ino - 1) / info->sb.block_group_size_inodes;
    //printf("inode block group number = %d\n", ino_block_group_number);
    uint32_t ino_inode_table_block = info->block_group_descriptor_table[ino_block_group_number].inode_table_block;
    //printf("inode table is at block %d\n", ino_inode_table_block);
    uint32_t ino_inode_index_in_group = (ino - 1) % info->sb.block_group_size_inodes;
    //printf("inode entry index in the group = %d\n", ino_inode_index_in_group);
    uint32_t ino_inode_block_in_group = (ino_inode_index_in_group * info->sb.inode_struct_size) / info->block_size;
    //printf("inode entry offset is %d blocks\n", ino_inode_block_in_group);
    uint32_t ino_inode_block_number = ino_inode_block_in_group + ino_inode_table_block;
    //printf("inode block number is %uth block\n", ino_inode_block_number);

    // Need to read the block to modify it
    if ((res = ext2_read_block(ext2, ino_inode_block_number, inode_block_buffer)) < 0) {
        return res;
    }

    uint32_t ino_entry_in_block = (ino_inode_index_in_group * info->sb.inode_struct_size) % info->block_size;
    memcpy(&inode_block_buffer[ino_entry_in_block], inode, info->sb.inode_struct_size);

    // Write the block back
    if ((res = ext2_write_block(ext2, ino_inode_block_number, inode_block_buffer)) < 0) {
        return res;
    }

    return 0;
}
