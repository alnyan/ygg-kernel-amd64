#pragma once
#include "sys/types.h"

ssize_t sys_read(int fd, void *data, size_t lim);
ssize_t sys_write(int fd, const void *data, size_t lim);
