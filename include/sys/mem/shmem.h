#pragma once
#include "sys/types.h"
#include "sys/list.h"
#include "sys/spin.h"

struct thread;

void *sys_mmap(void *hint, size_t length, int prot, int flags, int fd, off_t offset);
int sys_munmap(void *addr, size_t length);
