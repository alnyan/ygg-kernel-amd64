#pragma once

#include "sys/types.h"

#define MAP_FAILED          ((void *) -1)

#if !defined(__KERNEL__)
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
#endif
