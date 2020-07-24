/** vim: set ft=cpp.doxygen :
 * @file sys/debug.h
 * @brief Kernel debug output functions
 */
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include "user/time.h"

#define DEBUG_DEFAULT   0
#define DEBUG_INFO      1
#define DEBUG_WARN      2
#define DEBUG_ERROR     3
#define DEBUG_FATAL     4

#define DEBUG_SERIAL(bit)   (1 << (bit))
#define DEBUG_DISP(bit)     (1 << ((bit) + 8))
#define DEBUG_ALL_SERIAL    (DEBUG_SERIAL(DEBUG_DEFAULT) | \
                             DEBUG_SERIAL(DEBUG_INFO) | \
                             DEBUG_SERIAL(DEBUG_WARN) | \
                             DEBUG_SERIAL(DEBUG_ERROR) | \
                             DEBUG_SERIAL(DEBUG_FATAL))
#define DEBUG_ALL_DISP      (DEBUG_DISP(DEBUG_DEFAULT) | \
                             DEBUG_DISP(DEBUG_INFO) | \
                             DEBUG_DISP(DEBUG_WARN) | \
                             DEBUG_DISP(DEBUG_ERROR) | \
                             DEBUG_DISP(DEBUG_FATAL))

#define DEBUG_BASE_FMT      "[%08lu %s] "
#define DEBUG_BASE_ARGS     system_time / 1000000ULL, __func__

#define kdebug(f, ...)      debugf(DEBUG_DEFAULT,   DEBUG_BASE_FMT f, DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kinfo(f, ...)       debugf(DEBUG_INFO,      DEBUG_BASE_FMT f, DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kwarn(f, ...)       debugf(DEBUG_WARN,      "\033[1m\033[33m" DEBUG_BASE_FMT f "\033[0m", DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kerror(f, ...)      debugf(DEBUG_ERROR,     "\033[1m\033[31m" DEBUG_BASE_FMT f "\033[0m", DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kfatal(f, ...)      debugf(DEBUG_FATAL,     "\033[41m" DEBUG_BASE_FMT f "\033[0m", DEBUG_BASE_ARGS, ##__VA_ARGS__)
#define kprint(l, f, ...)   debugf(l,               DEBUG_BASE_FMT f, DEBUG_BASE_ARGS, ##__VA_ARGS__)

struct multiboot_tag_elf_sections;

void debug_symbol_table_set(uintptr_t symtab, uintptr_t strtab, size_t symtab_size, size_t strtab_size);
void debug_symbol_table_multiboot2(struct multiboot_tag_elf_sections *tag);
int debug_symbol_find(uintptr_t addr, const char **name, uintptr_t *base);
int debug_symbol_find_by_name(const char *name, uintptr_t *value);
void debug_backtrace(int level, uintptr_t rbp, int depth, int limit);

void fmtsiz(char *buf, size_t sz);

void debugc(int level, char c);
void debugs(int level, const char *s);

void debug_xs(uint64_t v, char *res, const char *set);
void debug_ds(int64_t x, char *res, int s, int sz);
void debug_init(void);
void debugf(int level, const char *fmt, ...);
void debugfv(int level, const char *fmt, va_list args);

void debug_dump(int level, const void *block, size_t count);
