#pragma once
#include "sys/types.h"

struct blkdev;
struct vnode;

int blk_enumerate_gpt(struct blkdev *dev, struct vnode *of, uint8_t *head);
