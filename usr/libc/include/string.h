#pragma once
#include <sys/types.h>

void *memcpy(void *restrict dst, const void *restrict src, size_t cnt);
void *memmove(void *dst, const void *src, size_t cnt);
void *memset(void *dst, int v, size_t count);
int memcmp(const void *a, const void *b, size_t count);

size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t lim);
int strcmp(const char *a, const char *b);
char *strchr(const char *a, int c);
char *strrchr(const char *a, int c);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t lim);
char *strcat(char *dst, const char *src);
char *strncat(char *dst, const char *src, size_t lim);
