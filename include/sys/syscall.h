/** vim: set ft=cpp.doxygen :
 * @file sys/syscall.h
 * @brief System call interface definitions
 */
#pragma once
#include "sys/types.h"

// Defined here so no need to include mm.h
#define userspace

// System call numbers

#define SYS_NR_READ         0
#define SYS_NR_WRITE        1

// System call signatures

/**
 * @brief 0 - Read data from file descriptor
 * @param fd Source file
 * @param buf Destination buffer. Has to be at least @ref count bytes
 * @param count Count of bytes to be read
 * @return Non-negative: count of bytes read
 *         Negative: error
 */
int sys_read(int fd, userspace void *buf, size_t count);

/**
 * @brief 1 - Write data to file descriptor
 * @param fd Destination file
 * @param buf Bytes to write
 * @param count Number of bytes to be written
 * @return Non-negative: count of bytes written
 *         Negative: error
 */
int sys_write(int fd, const userspace void *buf, size_t count);
