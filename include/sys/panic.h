#pragma once
#include <stdarg.h>
#include <stdint.h>

#define panic(f, ...) panicf("[%s] " f, __func__, ##__VA_ARGS__)

#if defined(ARCH_AMD64)
#define __idle_halt() asm volatile ("cli; hlt")
#endif

#define panic_hlt() do { __idle_halt(); } while (1)

void panicf(const char *fmt, ...) __attribute__((noreturn));
