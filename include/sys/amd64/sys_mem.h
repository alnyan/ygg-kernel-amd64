#pragma once
#include "sys/types.h"

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int sys_brk(void *addr);
