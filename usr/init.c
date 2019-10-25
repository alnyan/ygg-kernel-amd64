#include <stdint.h>

typedef uint64_t size_t;
typedef int64_t ssize_t;

#define ASM_REGISTER(name) \
        register uint64_t name asm (#name)

#define ASM_SYSCALL3(r0, r1, r2, r3)        ({ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        ASM_REGISTER(rdi) = (uint64_t) (r1); \
        ASM_REGISTER(rsi) = (uint64_t) (r2); \
        ASM_REGISTER(rdx) = (uint64_t) (r3); \
        asm volatile ("syscall":::"memory"); \
        rax; \
    })

ssize_t sys_write(int fd, const void *buf, size_t count) {
    return (ssize_t) ASM_SYSCALL3(1, fd, buf, count);
}

void _start(void) {
    sys_write(1, "Hello!\n", 7);

    while (1) {
    }
}
