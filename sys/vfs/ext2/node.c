#include "sys/fs/ext2/block.h"
#include "sys/fs/ext2/node.h"
#include "sys/fs/ext2/ext2.h"
#include "sys/fs/ofile.h"
#include "sys/fs/node.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/fs/fs.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/heap.h"

static int ext2_vnode_find(struct vnode *at, const char *name, struct vnode **res);
static ssize_t ext2_vnode_read(struct ofile *fd, void *buf, size_t count);

////

struct vnode_operations g_ext2_vnode_ops = {
    .find = ext2_vnode_find,

    .read = ext2_vnode_read,
};

////

static int ext2_vnode_find(struct vnode *at, const char *name, struct vnode **result) {
    _assert(at && at->type == VN_DIR);
    _assert(name);
    _assert(result);
    struct ext2_inode *at_inode = at->fs_data;
    _assert(at_inode);
    struct fs *ext2 = at->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    char block_buffer[data->block_size];
    uint32_t block_offset;
    uint32_t dir_blocks = at_inode->size_lower / data->block_size;
    size_t name_length = strlen(name);
    int res;

    // Because
    _assert(name_length < 255);

    for (uint32_t block_index = 0; block_index < dir_blocks; ++block_index) {
        block_offset = 0;

        if ((res = ext2_read_inode_block(ext2, at_inode, block_buffer, block_index)) != 0) {
            return res;
        }

        while (block_offset < data->block_size) {
            struct ext2_dirent *dirent = (struct ext2_dirent *) (block_buffer + block_offset);
            if (dirent->ino && (dirent->name_length_low == name_length)) {
                if (!strncmp(dirent->name, name, name_length)) {
                    // Found the dirent
                    struct ext2_inode *res_inode = kmalloc(data->inode_size);
                    _assert(res_inode);
                    struct vnode *node = vnode_create(VN_DIR, name);

                    if ((res = ext2_read_inode(ext2, res_inode, dirent->ino)) != 0) {
                        kfree(res_inode);
                        return res;
                    }

                    node->fs = ext2;
                    ext2_inode_to_vnode(node, res_inode, dirent->ino);

                    *result = node;
                    return 0;
                }
            }

            block_offset += dirent->ent_size;
        }
    }

    return -ENOENT;
}

static ssize_t ext2_vnode_read(struct ofile *fd, void *buf, size_t count) {
    _assert(fd);
    _assert(buf);
    struct vnode *node = fd->vnode;
    _assert(node);
    struct ext2_inode *inode = node->fs_data;
    _assert(inode);
    struct fs *ext2 = node->fs;
    _assert(ext2);
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    // TODO
    _assert(!inode->size_upper);

    if (fd->pos >= inode->size_lower) {
        return 0;
    }

    char block_buffer[data->block_size];
    size_t rem = MIN(count, inode->size_lower - fd->pos);
    size_t bread = 0;
    int res;

    while (rem) {
        size_t block_offset = fd->pos % data->block_size;
        size_t can_read = MIN(rem, data->block_size - block_offset);
        uint32_t block_index = fd->pos / data->block_size;

        if ((res = ext2_read_inode_block(ext2, inode, block_buffer, block_index)) != 0) {
            return res;
        }

        memcpy(buf, block_buffer + block_offset, can_read);

        buf += can_read;
        fd->pos += can_read;
        bread += can_read;
        rem -= can_read;
    }

    return bread;
}

////

// Map inode parameters to vnode fields
void ext2_inode_to_vnode(struct vnode *vnode, struct ext2_inode *inode, uint32_t ino) {
    vnode->ino = ino;
    vnode->uid = inode->uid;
    vnode->gid = inode->gid;
    vnode->mode = inode->mode & 0xFFF;
    vnode->fs_data = inode;
    vnode->op = &g_ext2_vnode_ops;

    switch (inode->mode & 0xF000) {
    case EXT2_IFDIR:
        vnode->type = VN_DIR;
        break;
    case EXT2_IFREG:
        vnode->type = VN_REG;
        break;
    default:
        panic("Unsupported inode type: %04x\n", inode->mode);
    }
}

