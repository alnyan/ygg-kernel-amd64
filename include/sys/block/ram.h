#pragma once
#include "sys/types.h"

struct blkdev;

#define RAM_IOC_GETBASE         1

extern struct blkdev *ramblk0;

void ramblk_init(uintptr_t at, size_t len);
