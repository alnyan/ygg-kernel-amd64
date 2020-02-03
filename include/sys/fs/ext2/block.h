#pragma once
#include "sys/types.h"

struct ext2_inode;
struct fs;

int ext2_read_superblock(struct fs *ext2);
int ext2_write_superblock(struct fs *ext2);

int ext2_read_block(struct fs *ext2, void *blk, uint32_t no);

int ext2_read_inode_block(struct fs *ext2, struct ext2_inode *inode, void *block, uint32_t index);

int ext2_read_inode(struct fs *ext2, struct ext2_inode *dst, uint32_t ino);
// NOTE: this function automatically updates access and modification time
//       on the inode written
int ext2_write_inode(struct fs *ext2, struct ext2_inode *node, uint32_t ino);
