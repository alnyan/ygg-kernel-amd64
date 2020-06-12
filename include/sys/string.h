/** vim: set ft=cpp.doxygen :
 * @file sys/string.h
 * @brief Generic string operations
 */
#pragma once
#include "sys/types.h"

// A macro to properly read data from misaligned pointers, byte by byte
// May be slower, but at least UBSan doesn't complain
#define realigned(base, field) ({ \
        typeof(base->field) __v0; \
        memcpy(&__v0, (void *) (base) + offsetof(typeof(*base), field), sizeof(__v0)); \
        __v0; \
    })

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

void *memcpy(void *restrict dst, const void *restrict src, size_t cnt);
uint64_t *memcpyq(uint64_t *restrict dst, const uint64_t *restrict src, size_t cnt);
void *memmove(void *dst, const void *src, size_t cnt);
void *memset(void *dst, int v, size_t count);
uint16_t *memsetw(uint16_t *dst, uint16_t v, size_t count);
uint32_t *memsetl(uint32_t *dst, uint32_t v, size_t count);
uint64_t *memsetq(uint64_t *dst, uint64_t v, size_t count);
int memcmp(const void *a, const void *b, size_t count);
void *memchr(const void *a, int c, size_t count);

size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t lim);
int strcmp(const char *a, const char *b);
char *strchr(const char *a, int c);
char *strrchr(const char *a, int c);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t lim);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t lim);
