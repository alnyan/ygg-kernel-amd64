#pragma once
#include "sys/types.h"

struct vnode_operations;
struct ext2_inode;
struct vnode;

extern struct vnode_operations g_ext2_vnode_ops;

void ext2_inode_to_vnode(struct vnode *vnode, struct ext2_inode *inode, uint32_t ino);
