/** vim: set ft=cpp.doxygen :
 * @file sys/mem.h
 * @brief Generic memory operations
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

void *memset(void *blk, int v, size_t sz);
void *memcpy(void *dst, const void *src, size_t sz);
