#pragma once
#include "sys/types.h"

#define EXT2_FLAG_RO            (1 << 0)

#define EXT2_STATE_CLEAN        1
#define EXT2_STATE_DIRTY        2

#define EXT2_ERROR_IGNORE       1
#define EXT2_ERROR_READONLY     2
#define EXT2_ERROR_PANIC        3

#define EXT2_REQ_COMP           (1 << 0)
#define EXT2_REQ_ENT_TYPE       (1 << 1)
#define EXT2_REQ_REPLAY_JOURNAL (1 << 2)
#define EXT2_REQ_JOURNAL_DEV    (1 << 3)

#define EXT2_RO_SPARSE_SB       (1 << 0)
#define EXT2_RO_64BIT_SIZE      (1 << 1)
#define EXT2_RO_DIR_BTREE       (1 << 2)

#define EXT2_SIGNATURE          0xEF53

#define EXT2_IFIFO              0x1000
#define EXT2_IFCHR              0x2000
#define EXT2_IFDIR              0x4000
#define EXT2_IFBLK              0x6000
#define EXT2_IFREG              0x8000
#define EXT2_IFLNK              0xA000
#define EXT2_IFSOCK             0xC000

#define EXT2_DIRECT_BLOCKS      12

struct vnode;

struct ext2_superblock {
    // Base superblock
    uint32_t inode_count;
    uint32_t block_count;
    uint32_t reserved_blocks;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t superblock_no;
    uint32_t log2_block_size;
    uint32_t log2_frag_size;
    uint32_t block_group_blocks;
    uint32_t block_group_frags;
    uint32_t block_group_inodes;
    uint32_t mount_time;
    uint32_t write_time;
    uint16_t mount_count;
    uint16_t fsck_mount_count;
    uint16_t ext2_signature;
    uint16_t state;
    uint16_t error_behavior;
    uint16_t version_minor;
    uint32_t fsck_time;
    uint32_t fsck_interval;
    uint32_t os_id;
    uint32_t version_major;
    uint16_t reserved_uid;
    uint16_t reserved_gid;
    // Extended superblock
    uint32_t first_inode;                   // == 11 in < 1.0
    uint16_t inode_size;                    // == 128 in < 1.0
    uint16_t superblock_block_group;        // For backup
    uint32_t optional_features;             // Don't matter
    uint32_t required_features;             // Cannot read without these
    uint32_t write_required_features;       // Can read without these
    char filesystem_id[16];
    char volume_name[16];
    char last_mount_path[64];
    uint32_t compression;
    uint8_t prealloc_file_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t __res0;
    char journal_id[16];
    uint32_t journal_inode;
    uint32_t journal_device;
    uint32_t orphan_inode_list_head;
} __attribute__((packed));

struct ext2_blkgrp_desc {
    uint32_t block_bitmap_no;
    uint32_t inode_bitmap_no;
    uint32_t inode_table_no;
    uint16_t free_blocks;
    uint16_t free_inodes;
    uint16_t directory_count;
    char __pad[14];
} __attribute__((packed));

struct ext2_inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size_lower;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t hard_links;
    uint32_t sector_count;
    uint32_t flags;
    uint32_t os_val1;
    uint32_t direct_blocks[EXT2_DIRECT_BLOCKS];
    uint32_t indirect_block_l1;
    uint32_t indirect_block_l2;
    uint32_t indirect_block_l3;
    uint32_t generation;
    uint32_t facl;
    union {
        uint32_t size_upper;
        uint32_t dir_acl;
    };
    uint32_t frag_block_no;
    uint32_t os_val2;
} __attribute__((packed));

struct ext2_dirent {
    uint32_t ino;
    uint16_t ent_size;
    uint8_t name_length_low;
    union {
        uint8_t type_indicator;
        uint8_t name_length_high;
    };
    char name[0];
} __attribute__((packed));

struct ext2_data {
    union {
        struct ext2_superblock sb;
        char __data[1024];
    };
    struct ext2_blkgrp_desc *bgdt;

    struct vnode *root;
    struct ext2_inode *root_inode;

    uint32_t flags;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t inodes_per_block;
    uint32_t blkgrp_inode_blocks;
    uint32_t bgdt_entry_count;
    uint32_t bgdt_size_blocks;
};
