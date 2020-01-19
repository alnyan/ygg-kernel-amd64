#include "sys/fs/ext2.h"
#include "sys/fs/vfs.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/errno.h"
#include "sys/heap.h"

enum vnode_type ext2_inode_type(struct ext2_inode *i) {
    uint16_t v = i->type_perm & 0xF000;
    switch (v) {
    case EXT2_TYPE_DIR:
        return VN_DIR;
    case EXT2_TYPE_REG:
        return VN_REG;
    case EXT2_TYPE_LNK:
        return VN_LNK;
    default:
        // fprintf(stderr, "Unknown file type: %x\n", v);
        // abort();
        panic("Unknown file type: %x\n", v);
        return 0;
    }
}

static int ext2_fs_init(struct fs *fs, const char *opt) {
    int res;
    kdebug("ext2_fs_init()\n");
    struct ext2_info *info;
    // ext2's private data is its superblock structure
    info = kmalloc(sizeof(struct ext2_info));
    fs->fs_private = info;

    // Read the superblock from blkdev
    if ((res = blk_read(fs->blk, info, EXT2_SBOFF, EXT2_SBSIZ)) != EXT2_SBSIZ) {
        kfree(fs->fs_private);

        kerror("ext2: superblock read failed\n");
        return -EINVAL;
    }

    // Check if superblock is ext2
    if (info->sb.magic != EXT2_MAGIC) {
        kfree(info);

        kdebug("ext2: magic mismatch\n");
        return -EINVAL;
    }

    // Check if we have an extended ext2 sb
    if (info->sb.version_major == 0) {
        // Initialize params which are missing in non-extended sbs
        info->sb.inode_struct_size = 128;
        info->sb.first_non_reserved = 11;
    }
    info->block_size = 1024 << info->sb.block_size_log;

    // Load block group descriptor table
    // Get descriptor table size
    uint32_t block_group_descriptor_table_length = info->sb.block_count / info->sb.block_group_size_blocks;
    if (block_group_descriptor_table_length * info->sb.block_group_size_blocks < info->sb.block_count) {
        ++block_group_descriptor_table_length;
    }
    info->block_group_count = block_group_descriptor_table_length;

    uint32_t block_group_descriptor_table_size_blocks = 32 * block_group_descriptor_table_length /
                                                        info->block_size + 1;

    uint32_t block_group_descriptor_table_block = 2;
    if (info->block_size > 1024) {
        block_group_descriptor_table_block = 1;
    }
    info->block_group_descriptor_table_block = block_group_descriptor_table_block;
    info->block_group_descriptor_table_size_blocks = block_group_descriptor_table_size_blocks;

    // Load all block group descriptors into memory
    kdebug("Allocating %u bytes for BGDT\n", info->block_group_descriptor_table_size_blocks * info->block_size);
    info->block_group_descriptor_table = kmalloc(info->block_group_descriptor_table_size_blocks * info->block_size);

    for (size_t i = 0; i < info->block_group_descriptor_table_size_blocks; ++i) {
        ext2_read_block(fs, i + info->block_group_descriptor_table_block,
                        (void *) (((uintptr_t) info->block_group_descriptor_table) + i * info->block_size));
    }

    return 0;
}

static int ext2_fs_umount(struct fs *fs) {
    struct ext2_info *info = fs->fs_private;
    // Free block group descriptor table
    kfree(info->block_group_descriptor_table);
    // Free superblock
    kfree(info);
    return 0;
}

static struct vnode *ext2_fs_get_root(struct fs *fs) {
    struct ext2_info *info = fs->fs_private;
    kdebug("ext2_fs_get_root()\n");

    struct ext2_inode *inode = kmalloc(info->sb.inode_struct_size);
    // Read root inode (2)
    if (ext2_read_inode(fs, inode, EXT2_ROOTINO) != 0) {
        kfree(inode);
        return NULL;
    }

    struct vnode *res = vnode_create(VN_DIR, NULL);

    res->ino = EXT2_ROOTINO;
    res->fs = fs;
    res->fs_data = inode;
    res->op = &ext2_vnode_ops;
    res->type = ext2_inode_type(inode);

    res->uid = inode->uid;
    res->gid = inode->gid;
    res->mode = inode->type_perm;

    return res;
}

static int ext2_fs_statvfs(struct fs *fs, struct statvfs *st) {
    struct ext2_info *info = fs->fs_private;

    st->f_blocks = info->sb.block_count;
    st->f_bfree = info->sb.free_block_count;
    st->f_bavail = info->sb.block_count - info->sb.su_reserved;

    st->f_files = info->sb.inode_count;
    st->f_ffree = info->sb.free_inode_count;
    st->f_favail = info->sb.inode_count - info->sb.first_non_reserved + 1;

    st->f_bsize = info->block_size;
    st->f_frsize = info->block_size;

    // XXX: put something here
    st->f_fsid = 0;
    st->f_flag = 0;
    st->f_namemax = 256;

    return 0;
}


static struct fs_class ext2_class = {
    .name = "ext2",
    .get_root = ext2_fs_get_root,
    .init = ext2_fs_init,
    .mount = NULL,
    .umount = ext2_fs_umount,
    .statvfs = ext2_fs_statvfs
};

void ext2_class_init(void) {
    fs_class_register(&ext2_class);
}

