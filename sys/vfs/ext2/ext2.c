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
    //case EXT2_TYPE_LNK:
    //    return VN_LNK;
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
    struct ext2_extsb *sb;
    // ext2's private data is its superblock structure
    sb = (struct ext2_extsb *) kmalloc(EXT2_SBSIZ);
    fs->fs_private = sb;

    // Read the superblock from blkdev
    if ((res = blk_read(fs->blk, sb, EXT2_SBOFF, EXT2_SBSIZ)) != EXT2_SBSIZ) {
        kfree(fs->fs_private);

        kerror("ext2: superblock read failed\n");
        return -EINVAL;
    }

    // Check if superblock is ext2
    if (sb->sb.magic != EXT2_MAGIC) {
        kfree(sb);

        kdebug("ext2: magic mismatch\n");
        return -EINVAL;
    }

    // Check if we have an extended ext2 sb
    if (sb->sb.version_major == 0) {
        // Initialize params which are missing in non-extended sbs
        sb->inode_struct_size = 128;
        sb->first_non_reserved = 11;
    }
    sb->block_size = 1024 << sb->sb.block_size_log;

    // Load block group descriptor table
    // Get descriptor table size
    uint32_t block_group_descriptor_table_length = sb->sb.block_count / sb->sb.block_group_size_blocks;
    if (block_group_descriptor_table_length * sb->sb.block_group_size_blocks < sb->sb.block_count) {
        ++block_group_descriptor_table_length;
    }
    sb->block_group_count = block_group_descriptor_table_length;

    uint32_t block_group_descriptor_table_size_blocks = 32 * block_group_descriptor_table_length /
                                                        sb->block_size + 1;

    uint32_t block_group_descriptor_table_block = 2;
    if (sb->block_size > 1024) {
        block_group_descriptor_table_block = 1;
    }
    sb->block_group_descriptor_table_block = block_group_descriptor_table_block;
    sb->block_group_descriptor_table_size_blocks = block_group_descriptor_table_size_blocks;

    // Load all block group descriptors into memory
    kdebug("Allocating %u bytes for BGDT\n", sb->block_group_descriptor_table_size_blocks * sb->block_size);
    sb->block_group_descriptor_table = (struct ext2_grp_desc *) kmalloc(sb->block_group_descriptor_table_size_blocks * sb->block_size);

    for (size_t i = 0; i < sb->block_group_descriptor_table_size_blocks; ++i) {
        ext2_read_block(fs, i + sb->block_group_descriptor_table_block,
                        (void *) (((uintptr_t) sb->block_group_descriptor_table) + i * sb->block_size));
    }

    return 0;
}

static int ext2_fs_umount(struct fs *fs) {
    struct ext2_extsb *sb = (struct ext2_extsb *) fs->fs_private;
    // Free block group descriptor table
    kfree(sb->block_group_descriptor_table);
    // Free superblock
    kfree(sb);
    return 0;
}

static struct vnode *ext2_fs_get_root(struct fs *fs) {
    // TODO: make sure get_root is only called once per filesystem
    //struct ext2_extsb *sb = fs->fs_private;
    //kdebug("ext2_fs_get_root()\n");

    //struct ext2_inode *inode = (struct ext2_inode *) kmalloc(sb->inode_struct_size);
    //// Read root inode (2)
    //if (ext2_read_inode(fs, inode, EXT2_ROOTINO) != 0) {
    //    kfree(inode);
    //    return NULL;
    //}

    //vnode_t *res = (vnode_t *) kmalloc(sizeof(vnode_t));

    //res->fs = fs;
    //res->fs_data = inode;
    //res->fs_number = EXT2_ROOTINO;
    //res->op = &ext2_vnode_ops;
    //res->type = ext2_inode_type(inode);

    //return res;
    panic("ext2 get root\n");
    return NULL;
}

static int ext2_fs_statvfs(struct fs *fs, struct statvfs *st) {
    struct ext2_extsb *sb = fs->fs_private;

    st->f_blocks = sb->sb.block_count;
    st->f_bfree = sb->sb.free_block_count;
    st->f_bavail = sb->sb.block_count - sb->sb.su_reserved;

    st->f_files = sb->sb.inode_count;
    st->f_ffree = sb->sb.free_inode_count;
    st->f_favail = sb->sb.inode_count - sb->first_non_reserved + 1;

    st->f_bsize = sb->block_size;
    st->f_frsize = sb->block_size;

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

