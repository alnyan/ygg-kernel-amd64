#pragma once
#include <stddef.h>
#include <stdint.h>

// General utils
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
void *memset(void *blk, int v, size_t sz);
void *memcpy(void *dst, const void *src, size_t sz);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t lim);

// Debug utils
void panic(const char *s);
void puts(const char *s);
void putx(uint64_t v);
void putc(char c);
