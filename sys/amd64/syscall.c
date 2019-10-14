#include "sys/types.h"
#include "sys/debug.h"

static ssize_t sys_read(int fd, void *buf, size_t lim);
static ssize_t sys_write(int fd, const void *buf, size_t lim);

uint8_t irq1_key = 0;

intptr_t amd64_syscall(uintptr_t rdi, uintptr_t rsi, uintptr_t rdx, uintptr_t rcx, uintptr_t r10, uintptr_t rax) {
    switch (rax) {
    case 0:
        return sys_read((int) rdi, (void *) rsi, (size_t) rdx);
    case 1:
        return sys_write((int) rdi, (const void *) rsi, (size_t) rdx);
    default:
        return -1;
    }
    return 0;
}

static ssize_t sys_read(int fd, void *buf, size_t lim) {
    irq1_key = 0;
    while (irq1_key == 0) {
        asm volatile ("hlt");
    }
    *((char *) buf) = irq1_key + '0' - 1;
    return 1;
}

static ssize_t sys_write(int fd, const void *buf, size_t lim) {
    kdebug("write(%d, ..., %lu)\n", fd, lim);
    for (size_t i = 0; i < lim; ++i) {
        debugc(DEBUG_DEFAULT, ((const char *) buf)[i]);
    }
    debugc(DEBUG_DEFAULT, '\n');
    return lim;
}
