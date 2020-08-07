#pragma once
#include "sys/types.h"

struct ext2_inode;
struct ext2_data;
struct fs;

uint32_t ext2_alloc_block(struct fs *fs, struct ext2_data *data);
void ext2_free_block(struct fs *fs, struct ext2_data *data, uint32_t block);
int ext2_file_resize(struct fs *fs, uint32_t ino, struct ext2_inode *inode, size_t new_size);
