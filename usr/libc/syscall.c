#include "bits/syscall.h"
#include "bits/global.h"
#include <errno.h>

#define ASM_REGISTER(name) \
        register uint64_t name asm (#name)

#define ASM_SYSCALL1(r0, r1)                ({ \
        ASM_REGISTER(rdi) = (uint64_t) (r1); \
        /*
         * Should be the last one because memory accesses for arguments
         * fuck up %rax
         */ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        asm volatile ("syscall":::"memory"); \
        rax; \
    })

#define ASM_SYSCALL2(r0, r1, r2)        ({ \
        ASM_REGISTER(rdi) = (uint64_t) (r1); \
        ASM_REGISTER(rsi) = (uint64_t) (r2); \
        /*
         * Should be the last one because memory accesses for arguments
         * fuck up %rax
         */ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        asm volatile ("syscall":::"memory", "rax", "rdi", "rsi", "rdx"); \
        rax; \
    })

#define ASM_SYSCALL3(r0, r1, r2, r3)        ({ \
        ASM_REGISTER(rdi) = (uint64_t) (r1); \
        ASM_REGISTER(rsi) = (uint64_t) (r2); \
        ASM_REGISTER(rdx) = (uint64_t) (r3); \
        /*
         * Should be the last one because memory accesses for arguments
         * fuck up %rax
         */ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        asm volatile ("syscall":::"memory", "rax", "rdi", "rsi", "rdx"); \
        rax; \
    })

#define SET_ERRNO(t, r)                     ({ \
        t res = (t) r; \
        if (res < 0) { \
            errno = -res; \
        } \
        res; \
    })

ssize_t write(int fd, const void *buf, size_t count) {
    return SET_ERRNO(ssize_t, ASM_SYSCALL3(SYSCALL_NR_WRITE, fd, buf, count));
}

ssize_t read(int fd, void *buf, size_t count) {
    return SET_ERRNO(ssize_t, ASM_SYSCALL3(SYSCALL_NR_READ, fd, buf, count));
}

int open(const char *filename, int flags, int mode) {
    return SET_ERRNO(int, ASM_SYSCALL3(SYSCALL_NR_OPEN, filename, flags, mode));
}

void close(int fd) {
    (void) ASM_SYSCALL1(SYSCALL_NR_CLOSE, fd);
}

int stat(const char *filename, struct stat *st) {
    return SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NR_STAT, filename, st));
}

int brk(void *addr) {
    return SET_ERRNO(int, ASM_SYSCALL1(SYSCALL_NR_BRK, addr));
}

__attribute__((noreturn)) void exit(int code) {
    (void) ASM_SYSCALL1(SYSCALL_NR_EXIT, code);
    while (1);
}

// Although sbrk() is implemented in userspace, I guess it should also be here
void *sbrk(intptr_t inc) {
    if (inc == 0) {
        return __cur_brk;
    }

    void *new_brk = (void *) ((uintptr_t) __cur_brk + inc);
    printf("New brk = %p\n", new_brk);

    if (brk(new_brk) != 0) {
        return __cur_brk;
    } else {
        return __cur_brk = new_brk;
    }
}
