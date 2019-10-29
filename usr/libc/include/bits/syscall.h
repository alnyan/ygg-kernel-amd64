#pragma once
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int open(const char *filename, int flags, int mode);
void close(int fd);
int stat(const char *filename, struct stat *st);

__attribute__((noreturn)) void exit(int code);
