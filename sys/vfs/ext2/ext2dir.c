// ext2fs directory content operations
#include "sys/fs/ext2.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/mem.h"

// #include <string.h>
// #include <stdlib.h>
// #include <assert.h>
// #include <errno.h>
// #include <stdio.h>

// Add an inode to directory
int ext2_dir_add_inode(fs_t *ext2, vnode_t *dir, const char *name, uint32_t ino) {
    struct ext2_extsb *sb = (struct ext2_extsb *) ext2->fs_private;
    char block_buffer[sb->block_size];
    struct ext2_inode *dir_inode = dir->fs_data;
    struct ext2_dirent *current_dirent, *result_dirent;
    int res;

    size_t req_free = strlen(name) + sizeof(struct ext2_dirent);
    // Align up 4 bytes
    req_free = (req_free + 3) & ~3;

    // Try reading parent dirent blocks to see if any has
    // some space to fit our file
    size_t dir_size_blocks = (dir_inode->size_lower + sb->block_size - 1) / sb->block_size;
    for (size_t i = 0; i < dir_size_blocks; ++i) {
        current_dirent = NULL;
        result_dirent = NULL;
        size_t off = 0;

        // Read directory content block
        if ((res = ext2_read_inode_block(ext2, dir_inode, i, block_buffer)) < 0) {
            return res;
        }

        // Check if any of the entries can be split to fit our entry
        while (off < sb->block_size) {
            current_dirent = (struct ext2_dirent *) &block_buffer[off];
            if (current_dirent->ino == 0) {
                kwarn("ext2: found dirent with ino = 0\n");
            }

            // Check how much space we need to still store the entry
            size_t real_len = current_dirent->name_len + sizeof(struct dirent);
            real_len = (real_len + 3) & ~3;

            // And check how much is left to fit our entry
            if (real_len < current_dirent->len /* Sanity? */ &&
                current_dirent->len - real_len >= req_free) {
                // Yay, can fit our dirent in there

                // Sanity check that we're aligned properly
                _assert(((off + real_len) & 3) == 0);
                result_dirent = (struct ext2_dirent *) &block_buffer[off + real_len];
                result_dirent->len = current_dirent->len - real_len;
                result_dirent->name_len = strlen(name);
                result_dirent->type_ind = 0;
                result_dirent->ino = ino;
                strncpy(result_dirent->name, name, result_dirent->name_len);
                current_dirent->len = real_len;

                if ((res = ext2_write_inode_block(ext2, dir_inode, i, block_buffer)) < 0) {
                    return res;
                }

                return 0;
            }

            off += current_dirent->len;
        }
    }

    dir_inode->size_lower += sb->block_size;
    if ((res = ext2_inode_alloc_block(ext2, dir_inode, dir->fs_number, dir_size_blocks)) < 0) {
        dir_inode->size_lower -= sb->block_size;
        return res;
    }

    memset(block_buffer, 0, sb->block_size);
    current_dirent = (struct ext2_dirent *) block_buffer;
    current_dirent->ino = ino;
    current_dirent->len = sb->block_size;
    current_dirent->name_len = strlen(name);
    current_dirent->type_ind = 0;
    strncpy(current_dirent->name, name, current_dirent->name_len);

    return ext2_write_inode_block(ext2, dir_inode, dir_size_blocks, block_buffer);
}

// Not only free the block itself, but also remove it from index list
static int ext2_free_block_index(fs_t *ext2, struct ext2_inode *inode, uint32_t index, uint32_t ino, size_t sz) {
    if (index >= 12) {
        // TODO: Implement this
        panic("Not implemented\n");
    }

    int res;
    uint32_t block_no = inode->direct_blocks[index];

    if ((res = ext2_free_block(ext2, block_no)) < 0) {
        return res;
    }

    // Shift direct indexed blocks
    for (uint32_t i = index; i < 11; ++i) {
        inode->direct_blocks[i] = inode->direct_blocks[i + 1];
    }
    // TODO: inode->direct_blocks[11] becomes the first block of indirect block
    inode->direct_blocks[11] = 0;

    inode->size_lower -= sz;

    return ext2_write_inode(ext2, inode, ino);
}

int ext2_dir_remove_inode(fs_t *ext2, vnode_t *dir, const char *name, uint32_t ino) {
    struct ext2_extsb *sb = (struct ext2_extsb *) ext2->fs_private;
    char block_buffer[sb->block_size];
    struct ext2_inode *dir_inode = dir->fs_data;
    struct ext2_dirent *current_dirent, *prev_dirent;
    int res;

    size_t dir_size_blocks = (dir_inode->size_lower + sb->block_size - 1) / sb->block_size;

    for (size_t i = 0; i < dir_size_blocks; ++i) {
        if ((res = ext2_read_inode_block(ext2, dir_inode, i, block_buffer)) < 0) {
            return res;
        }

        size_t off = 0;
        current_dirent = NULL;
        prev_dirent = NULL;

        while (off < sb->block_size) {
            prev_dirent = current_dirent;
            current_dirent = (struct ext2_dirent *) &block_buffer[off];

            if (current_dirent->ino == 0) {
                kwarn("ext2: found dirent with ino = 0\n");
            }

            if (current_dirent->name_len == strlen(name) &&
                !strncmp(current_dirent->name, name, current_dirent->name_len)) {
                // Found matching dirent
                // Sanity
                _assert(current_dirent->ino == ino);

                if (current_dirent->len + off >= sb->block_size) {
                    // It's the last node in the list
                    if (!prev_dirent) {
                        return ext2_free_block_index(ext2, dir_inode, i, dir->fs_number, sb->block_size);
                    }

                    // Resize the previous node
                    prev_dirent->len += current_dirent->len;
                    return ext2_write_inode_block(ext2, dir_inode, i, block_buffer);
                } else {
                    // It's not the last one - relocate the next entry
                    uint32_t len = current_dirent->len;
                    struct ext2_dirent *next_dirent = (struct ext2_dirent *) &block_buffer[off + len];
                    memmove(current_dirent, next_dirent, next_dirent->len);
                    next_dirent = current_dirent;
                    next_dirent->len += len;
                    _assert(((off + len) & 3) == 0);
                    return ext2_write_inode_block(ext2, dir_inode, i, block_buffer);
                }
            }

            off += current_dirent->len;
        }
    }

    return -EIO;
}
