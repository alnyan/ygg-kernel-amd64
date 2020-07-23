#pragma once
#include "sys/types.h"
#include <stdbool.h>

struct vnode;

struct vnode *ram_vnode_create(enum vnode_type t, const char *name);

int ram_vnode_bset_resize(struct vnode *vn, size_t size);
int ram_vnode_bset_set(struct vnode *vn, size_t index, uintptr_t block_ptr);
uintptr_t ram_vnode_bset_get(struct vnode *vn, size_t index);
