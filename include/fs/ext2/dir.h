#pragma once
#include "sys/types.h"

struct ext2_inode;
struct fs;
enum vnode_type;
struct vnode;

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

int ext2_dir_insert_inode(struct fs *ext2,
                          struct ext2_inode *at_inode,
                          uint32_t at_ino,
                          const char *name,
                          uint32_t ino,
                          enum vnode_type type);
int ext2_dir_del_inode(struct fs *ext2,
                       struct ext2_inode *at_inode,
                       uint32_t at_ino,
                       struct vnode *vn);
