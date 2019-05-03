#include "panic.h"
#include "debug.h"

void panicf(const char *fmt, ...) {
    va_list args;
    kfatal("--- Panic ---\n");

    va_start(args, fmt);
    debugfv(DEBUG_FATAL, fmt, args);
    va_end(args);

    kfatal("--- Panic ---\n");

    panic_hlt();
}
