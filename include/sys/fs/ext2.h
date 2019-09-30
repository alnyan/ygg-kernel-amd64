#pragma once
#include <stdint.h>
#include "fs.h"

#define EXT2_MAGIC      ((uint16_t) 0xEF53)

#define EXT2_SBSIZ      1024
#define EXT2_SBOFF      1024

#define EXT2_ROOTINO    2

#define EXT2_GOOD       ((uint16_t) 1)
#define EXT2_BAD        ((uint16_t) 2)

#define EXT2_EACT_IGN   ((uint16_t) 1)
#define EXT2_EACT_REM   ((uint16_t) 2)
#define EXT2_EACT_PAN   ((uint16_t) 3)

#define EXT2_TYPE_REG   ((uint16_t) 0x8000)
#define EXT2_TYPE_DIR   ((uint16_t) 0x4000)
#define EXT2_TYPE_LNK   ((uint16_t) 0xA000)

struct ext2_sb {
    uint32_t inode_count;
    uint32_t block_count;
    uint32_t su_reserved;
    uint32_t free_block_count;
    uint32_t free_inode_count;
    uint32_t sb_block_number;
    uint32_t block_size_log;
    uint32_t frag_size_log;
    uint32_t block_group_size_blocks;
    uint32_t block_group_size_frags;
    uint32_t block_group_size_inodes;
    uint32_t last_mount_time;
    uint32_t last_mtime;
    uint16_t mount_count_since_fsck;
    uint16_t mount_max_before_fsck;
    uint16_t magic;
    uint16_t fs_state;
    uint16_t error_action;
    uint16_t version_minor;
    uint32_t last_fsck_time;
    uint32_t os_id;
    uint32_t version_major;
    uint16_t su_uid;
    uint16_t su_gid;
} __attribute__((packed));

struct ext2_extsb {
    struct ext2_sb sb;
    uint32_t first_non_reserved;
    uint16_t inode_struct_size;
    uint16_t backup_group_number;
    uint32_t optional_features;
    uint32_t required_features;
    uint32_t ro_required_features;
    char fsid[16];
    char volname[16];
    char last_mount_path[64];
    uint32_t compression;
    uint8_t prealloc_file_block_number;
    uint8_t prealloc_dir_block_number;
    uint16_t __un0;
    char journal_id[16];
    uint32_t journal_inode;
    uint32_t journal_dev;
    uint32_t orphan_inode_head;

    // driver-specific info
    uint32_t block_size;
    uint32_t block_group_count;
    uint32_t block_group_descriptor_table_block;
    uint32_t block_group_descriptor_table_size_blocks;
    struct ext2_grp_desc *block_group_descriptor_table;
} __attribute__((packed));

struct ext2_grp_desc {
    uint32_t block_usage_bitmap_block;
    uint32_t inode_usage_bitmap_block;
    uint32_t inode_table_block;
    uint16_t free_blocks;
    uint16_t free_inodes;
    uint16_t dir_count;
    char __pad[14];
} __attribute__((packed));

struct ext2_inode {
    uint16_t type_perm;
    uint16_t uid;
    uint32_t size_lower;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t hard_link_count;
    uint32_t disk_sector_count;
    uint32_t flags;
    uint32_t os_value_1;
    uint32_t direct_blocks[12];
    uint32_t l1_indirect_block;
    uint32_t l2_indirect_block;
    uint32_t l3_indirect_block;
    uint32_t gen_number;
    uint32_t acl;
    union {
        uint32_t dir_acl;
        uint32_t size_upper;
    };
    uint32_t frag_block_addr;
    char os_value_2[12];
} __attribute__((packed));

struct ext2_dirent {
    uint32_t ino;
    uint16_t len;
    uint8_t name_len;
    uint8_t type_ind;
    char name[];
} __attribute__((packed));

void ext2_class_init(void);
enum vnode_type ext2_inode_type(struct ext2_inode *i);

// Implemented in ext2blk.c
int ext2_write_superblock(fs_t *ext2);
int ext2_read_block(fs_t *ext2, uint32_t block_no, void *buf);
int ext2_write_block(fs_t *ext2, uint32_t block_no, const void *buf);
int ext2_read_inode_block(fs_t *ext2, struct ext2_inode *inode, uint32_t index, void *buf);
int ext2_write_inode_block(fs_t *ext2, struct ext2_inode *inode, uint32_t index, const void *buf);
int ext2_read_inode(fs_t *ext2, struct ext2_inode *inode, uint32_t ino);
int ext2_write_inode(fs_t *ext2, const struct ext2_inode *inode, uint32_t ino);

// Implemented in ext2alloc.c
int ext2_alloc_block(fs_t *ext2, uint32_t *block_no);
int ext2_free_block(fs_t *ext2, uint32_t block_no);
int ext2_inode_alloc_block(fs_t *ext2, struct ext2_inode *inode, uint32_t ino, uint32_t index);
int ext2_free_inode_block(fs_t *ext2, struct ext2_inode *inode, uint32_t ino, uint32_t index);
int ext2_free_inode(fs_t *ext2, uint32_t ino);
int ext2_alloc_inode(fs_t *ext2, uint32_t *ino);

// Implemented in ext2dir.c
int ext2_dir_add_inode(fs_t *ext2, vnode_t *dir, const char *name, uint32_t ino);
int ext2_dir_remove_inode(fs_t *ext2, vnode_t *dir, const char *name, uint32_t ino);

extern struct vnode_operations ext2_vnode_ops;
