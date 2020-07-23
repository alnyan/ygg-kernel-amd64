#pragma once
struct fs;
struct vnode_operations;

int tar_init(struct fs *ramfs, void *mem_base);
