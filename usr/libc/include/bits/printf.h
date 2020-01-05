#pragma once
#include <sys/types.h>
#include <stdarg.h>

int printf(const char *fmt, ...);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
