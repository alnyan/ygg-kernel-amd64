#include "sys/panic.h"
#include "sys/debug.h"

void panicf(const char *fmt, ...) {
    asm volatile ("cli");
    va_list args;
    kfatal("--- Panic ---\n");

    va_start(args, fmt);
    debugs(DEBUG_FATAL, "\033[41m");
    debugfv(DEBUG_FATAL, fmt, args);
    debugs(DEBUG_FATAL, "\033[0m");
    va_end(args);

    kfatal("--- Panic ---\n");

    panic_hlt();
}
