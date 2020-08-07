#pragma once
#include "sys/types.h"
#include "sys/list.h"
#include "sys/spin.h"

struct thread;

int sys_shmget(size_t size, int flags);
void *sys_shmat(int id, const void *hint, int flags);

void *sys_mmap(void *hint, size_t length, int prot, int flags, int fd, off_t offset);
int sys_munmap(void *addr, size_t length);
