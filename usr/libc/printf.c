#include "bits/printf.h"
#include <unistd.h>
#define PRINTF_BUFFER_SIZE  1024

int printf(const char *fmt, ...) {
    char buf[PRINTF_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(buf, PRINTF_BUFFER_SIZE, fmt, args);
    va_end(args);

    // TODO: use libc's FILE * instead
    return write(STDOUT_FILENO, buf, res);
}

int snprintf(char *buf, size_t lim, const char *fmt, ...) {
    va_list args;
    int res;

    va_start(args, fmt);
    res = vsnprintf(buf, PRINTF_BUFFER_SIZE, fmt, args);
    va_end(args);

    return res;
}
