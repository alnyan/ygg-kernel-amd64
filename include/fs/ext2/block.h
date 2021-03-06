#pragma once
#include "sys/types.h"

struct ext2_inode;
struct fs;

int ext2_read_superblock(struct fs *ext2);
int ext2_write_superblock(struct fs *ext2);

int ext2_read_block(struct fs *ext2, void *blk, uint32_t no);
int ext2_write_block(struct fs *ext2, const void *blk, uint32_t no);

int ext2_read_inode_block(struct fs *ext2, struct ext2_inode *inode, void *block, uint32_t index);
int ext2_write_inode_block(struct fs *ext2, struct ext2_inode *inode, const void *block, uint32_t index);

uint32_t ext2_inode_get_index(struct fs *ext2, struct ext2_inode *inode, uint32_t index);
int ext2_inode_set_index(struct fs *ext2, struct ext2_inode *inode, uint32_t ino, uint32_t index, uint32_t value);

int ext2_read_inode(struct fs *ext2, struct ext2_inode *dst, uint32_t ino);
// NOTE: this function automatically updates access and modification time
//       on the inode written
int ext2_write_inode(struct fs *ext2, struct ext2_inode *node, uint32_t ino);
