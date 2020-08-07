#include "fs/ext2/block.h"
#include "fs/ext2/ext2.h"
#include "fs/ext2/node.h"
#include "fs/fs.h"
#include "fs/node.h"
#include "sys/assert.h"
#include "sys/attr.h"
#include "sys/block/blk.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/mem/slab.h"
#include "sys/string.h"
#include "user/errno.h"
#include "user/mount.h"

static int ext2_init(struct fs *ext2, const char *opt);
static void ext2_destroy(struct fs *ext2);
static struct vnode *ext2_get_root(struct fs *ext2);

////

static struct fs_class g_ext2 = {
    .name = "ext2",
    .opt = 0,
    .init = ext2_init,
    .destroy = ext2_destroy,
    .get_root = ext2_get_root
};

////

static int ext2_init(struct fs *ext2, const char *opt) {
    _assert(ext2->blk);

    struct ext2_data *data = kmalloc(sizeof(struct ext2_data));
    _assert(data);
    ext2->fs_private = data;

    int res;

    if (!(ext2->flags & MS_SYNCHRONOUS)) {
        // 16MiB cache
        blk_set_cache(ext2->blk, 4096);
    } else {
        kdebug("Not using cache\n");
    }

    // Read superblock
    if ((res = ext2_read_superblock(ext2)) != 0) {
        kfree(data);
        ext2->fs_private = NULL;
        return res;
    }

    // Check the signature
    if (data->sb.ext2_signature != EXT2_SIGNATURE) {
        kerror("Invalid ext2 signature\n");
        kfree(data);
        ext2->fs_private = NULL;
        return -EINVAL;
    }

    // TODO: perform some kind of fsck?
    kinfo("Volume ID: \"%s\"\n", data->sb.volume_name);
    kinfo("Filesystem version: %d.%d\n", data->sb.version_major, data->sb.version_minor);

    // Calculate filesystem parameters
    data->block_size = 1024 << data->sb.log2_block_size;

    if (data->sb.version_major != 0) {
        data->inode_size = data->sb.inode_size;
    } else {
        data->inode_size = 128;
    }

    // Create a slab cache for inode objects
    data->inode_cache = slab_cache_get(data->inode_size);
    if (!data->inode_cache) {
        // TODO: error handling
        panic("Failed to allocate a slab cache of size %d for inodes\n", data->inode_size);
    }

    data->inodes_per_block = data->block_size / data->inode_size;
    data->blkgrp_inode_blocks = data->sb.block_group_inodes / data->inodes_per_block;

    kinfo("Block size: %u, inode size: %u\n", data->block_size, data->inode_size);

    data->bgdt_entry_count = (data->sb.block_count + data->sb.block_group_blocks - 1) / data->sb.block_group_blocks;
    uint32_t bgdt_size = data->bgdt_entry_count * sizeof(struct ext2_blkgrp_desc);
    data->bgdt_size_blocks = (bgdt_size + data->block_size - 1) / data->block_size;

    kinfo("Block groups: %u\n", data->bgdt_entry_count);
    kinfo("BGDT occupies %u block(s)\n", data->bgdt_size_blocks);

    data->bgdt = kmalloc(data->bgdt_size_blocks * data->block_size);
    _assert(data->bgdt);

    // BGDT starts with block 2
    for (uint32_t i = 0; i < data->bgdt_size_blocks; ++i) {
        if ((res = ext2_read_block(ext2, ((void *) data->bgdt) + i * data->block_size, i + 2)) != 0) {
            kfree(data->bgdt);
            kfree(data);
            ext2->fs_private = NULL;
            return res;
        }
    }

    data->root_inode = slab_calloc(data->inode_cache);
    _assert(data->root_inode);
    data->root = vnode_create(VN_DIR, NULL);
    _assert(data->root);

    // Read root inode
    if ((res = ext2_read_inode(ext2, data->root_inode, 2)) != 0) {
        kfree(data->root_inode);
        kfree(data->root);
        kfree(data->bgdt);
        kfree(data);
        return res;
    }

    data->root->fs = ext2;
    ext2_inode_to_vnode(data->root, data->root_inode, 2);

    return 0;
}

static void ext2_destroy(struct fs *ext2) {
    struct ext2_data *data = ext2->fs_private;
    _assert(data);

    if (!(ext2->flags & MS_SYNCHRONOUS)) {
        blk_cache_release(ext2->blk);
    }

    kfree(data->root_inode);
    kfree(data->root);
    kfree(data->bgdt);
    kfree(data);
}

static struct vnode *ext2_get_root(struct fs *ext2) {
    struct ext2_data *data = ext2->fs_private;
    _assert(data);
    _assert(data->root);

    return data->root;
}

__init(ext2_class_init) {
    fs_class_register(&g_ext2);
}
