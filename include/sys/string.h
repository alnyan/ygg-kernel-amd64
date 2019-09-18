/** vim: set ft=cpp.doxygen :
 * @file sys/string.h
 * @brief Generic string operations
 */
#pragma once
#include <stddef.h>

size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t lim);
