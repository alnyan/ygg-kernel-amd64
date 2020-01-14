/** vim: set ft=cpp.doxygen :
 * @file sys/debug.h
 * @brief Kernel debug output functions
 */
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#define DEBUG_DEFAULT   0
#define DEBUG_INFO      1
#define DEBUG_WARN      2
#define DEBUG_ERROR     3
#define DEBUG_FATAL     4

#define DEBUG_OUT_SERIAL    0
#define DEBUG_OUT_DISP      1

#define kdebug(f, ...)      debugf(DEBUG_DEFAULT,   "[%s] " f, __func__, ##__VA_ARGS__)
#define kinfo(f, ...)       debugf(DEBUG_INFO,      "[%s] " f, __func__, ##__VA_ARGS__)
#define kwarn(f, ...)       debugf(DEBUG_WARN,      "\033[1m\033[33m[%s] " f "\033[0m", __func__, ##__VA_ARGS__)
#define kerror(f, ...)      debugf(DEBUG_ERROR,     "\033[1m\033[31m[%s] " f "\033[0m", __func__, ##__VA_ARGS__)
#define kfatal(f, ...)      debugf(DEBUG_FATAL,     "\033[41m[%s] " f "\033[0m", __func__, ##__VA_ARGS__)
#define kprint(l, f, ...)   debugf(l,               "[%s] " f, __func__, ##__VA_ARGS__)

void fmtsiz(char *buf, size_t sz);

void debugc(int level, char c);
void debugs(int level, const char *s);

void debug_xs(uint64_t v, char *res, const char *set);
void debug_ds(int64_t x, char *res, int s, int sz);
void debug_init(void);
void debugf(int level, const char *fmt, ...);
void debugfv(int level, const char *fmt, va_list args);

void debug_dump(int level, const void *block, size_t count);
