#pragma once
#include "sys/blk.h"

extern struct blkdev *ramblk0;

void ramblk_init(uintptr_t at, size_t len);
