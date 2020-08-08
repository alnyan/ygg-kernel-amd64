#include "fs/ext2/alloc.h"
#include "fs/ext2/block.h"
#include "fs/ext2/dir.h"
#include "fs/ext2/ext2.h"
#include "fs/fs.h"
#include "fs/node.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "user/errno.h"

int ext2_dir_insert_inode(struct fs *ext2,
                          struct ext2_inode *at_inode,
                          uint32_t at_ino,
                          const char *name,
                          uint32_t ino,
                          enum vnode_type type) {
    // Align name
    // Not zero-terminated
    struct ext2_data *data = ext2->fs_private;
    size_t size_needed = ((strlen(name) + 3) & ~3) + sizeof(struct ext2_dirent);
    size_t block_count = at_inode->size_lower / data->block_size;
    struct ext2_dirent *new = NULL;
    int inode_changed = 0;
    uint32_t index;
    char buf[data->block_size];

    // Try to fit inside already-existing blocks
    for (size_t i = 0; i < block_count; ++i) {
        // Find out how much space is free in this dir
        _assert(ext2_read_inode_block(ext2, at_inode, buf, i) == 0);

        size_t offset = 0;
        while (offset < data->block_size) {
            struct ext2_dirent *ent = (struct ext2_dirent *) &buf[offset];
            // Minimum size needed for this entry
            size_t size_needed_prev = sizeof(struct ext2_dirent);
            if (ent->ino) {
                // A real inode dirent
                size_needed_prev += ((ent->name_length_low + 3) & ~3);
            }
            _assert(ent->ent_size >= size_needed_prev);
            size_t spare_space = ent->ent_size - size_needed_prev;

            if (spare_space >= size_needed) {
                // Cut off the spare space from previous entry
                ent->ent_size = size_needed_prev;

                // This will be the new entry
                new = (struct ext2_dirent *) &buf[offset + ent->ent_size];
                new->ent_size = spare_space;
                index = i;

                break;
            }

            offset += ent->ent_size;
        }

        if (new) {
            break;
        }
    }

    if (!new) {
        // Grow the directory
        _assert(ext2_file_resize(ext2, at_ino, at_inode, at_inode->size_lower + data->block_size) == 0);

        inode_changed = 1;
        memset(buf, 0, data->block_size);

        // New entry takes the whole block
        new = (struct ext2_dirent *) buf;
        new->ent_size = data->block_size;

        index = block_count;
    }

    new->ino = ino;
    new->name_length_low = strlen(name);
    strncpy(new->name, name, new->name_length_low);
    // TODO
    if (data->sb.required_features & EXT2_REQ_ENT_TYPE) {
        switch (type) {
        case VN_REG:
            new->type_indicator = 1;
            break;
        case VN_DIR:
            new->type_indicator = 2;
            break;
        default:
            panic("Unsupported vnode type: %02x\n", type);
        }
    } else {
        new->name_length_high = 0;
    }

    // Write back
    _assert(ext2_write_inode_block(ext2, at_inode, buf, index) == 0);

    if (inode_changed) {
        _assert(ext2_write_inode(ext2, at_inode, at_ino) == 0);
    }

    return 0;
}

int ext2_dir_del_inode(struct fs *ext2, struct ext2_inode *at_inode, uint32_t at_ino, struct vnode *vn) {
    size_t offset;
    struct ext2_data *data = ext2->fs_private;
    size_t block_count = at_inode->size_lower / data->block_size;
    char buf[data->block_size];
    struct ext2_dirent *prev, *next;

    for (size_t i = 0; i < block_count; ++i) {
        offset = 0;
        prev = NULL;

        _assert(ext2_read_inode_block(ext2, at_inode, buf, i) == 0);

        while (offset < data->block_size) {
            struct ext2_dirent *ent = (struct ext2_dirent *) &buf[offset];

            if (ent->ino == vn->ino) {
                _assert(!strncmp(ent->name, vn->name, ent->name_length_low));

                ent->ino = 0;

                // TODO: Free directory blocks when the last block entry
                //       is removed
                // TODO: would be better to just "lshift" all the following
                //       entries to reduce fragmentation
                if (ent->ent_size == data->block_size) {
                    kinfo("TODO: free directory blocks\n");
                    return ext2_write_inode_block(ext2, at_inode, buf, i);
                }

                if (prev) {
                    prev->ent_size += ent->ent_size;
                    return ext2_write_inode_block(ext2, at_inode, buf, i);
                } else {
                    _assert(!offset);
                    _assert(ent->ent_size < data->block_size);

                    next = (struct ext2_dirent *) &buf[offset + ent->ent_size];
                    size_t old = ent->ent_size;
                    size_t tot = ent->ent_size + next->ent_size;
                    size_t len = next->ent_size;

                    memmove(ent, next, len);

                    ent->ent_size = tot;
                    return ext2_write_inode_block(ext2, at_inode, buf, i);
                }
            }

            prev = ent;
            offset += ent->ent_size;
        }
    }

    return -ENOENT;
}
