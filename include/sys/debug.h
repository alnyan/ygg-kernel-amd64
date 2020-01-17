/** vim: set ft=cpp.doxygen :
 * @file sys/debug.h
 * @brief Kernel debug output functions
 */
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include "sys/time.h"

#define DEBUG_DEFAULT   0
#define DEBUG_INFO      1
#define DEBUG_WARN      2
#define DEBUG_ERROR     3
#define DEBUG_FATAL     4

#define DEBUG_OUT_SERIAL    0
#define DEBUG_OUT_DISP      1

#define DEBUG_BASE_FMT      "[%08lu %s] "
#define DEBUG_BASE_ARGS     system_time / 1000000ULL, __func__

#define kdebug(f, ...)      debugf(DEBUG_DEFAULT,   DEBUG_BASE_FMT f, DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kinfo(f, ...)       debugf(DEBUG_INFO,      DEBUG_BASE_FMT f, DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kwarn(f, ...)       debugf(DEBUG_WARN,      "\033[1m\033[33m" DEBUG_BASE_FMT f "\033[0m", DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kerror(f, ...)      debugf(DEBUG_ERROR,     "\033[1m\033[31m" DEBUG_BASE_FMT f "\033[0m", DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kfatal(f, ...)      debugf(DEBUG_FATAL,     "\033[41m" DEBUG_BASE_FMT f "\033[0m", DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kprint(l, f, ...)   debugf(l,               DEBUG_BASE_FMT f, DEBUG_BASE_ARGS, ##__VA_ARGS__)

void fmtsiz(char *buf, size_t sz);

void debugc(int level, char c);
void debugs(int level, const char *s);

void debug_xs(uint64_t v, char *res, const char *set);
void debug_ds(int64_t x, char *res, int s, int sz);
void debug_init(void);
void debugf(int level, const char *fmt, ...);
void debugfv(int level, const char *fmt, va_list args);

void debug_dump(int level, const void *block, size_t count);
