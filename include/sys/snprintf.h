#pragma once
#include <stdarg.h>
#include "sys/types.h"

int snprintf(char *buf, size_t lim, const char *fmt, ...);
int vsnprintf(char *buf, size_t lim, const char *fmt, va_list args);
