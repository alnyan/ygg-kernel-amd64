#pragma once
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int open(const char *filename, int flags, int mode);
void close(int fd);
int stat(const char *filename, struct stat *st);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, uintptr_t offset);

__attribute__((noreturn)) void exit(int code);
