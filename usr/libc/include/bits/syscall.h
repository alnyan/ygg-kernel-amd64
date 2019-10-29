#pragma once
#include <sys/types.h>

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
__attribute__((noreturn)) void exit(int code);
