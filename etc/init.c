#include <sys/types.h>
#include <stdint.h>

#define SYSCALL3(n, a, b, c) ({ \
    register uint64_t rax asm ("rax") = (uint64_t) (n); \
    register uint64_t rdi asm ("rdi") = (uint64_t) (a); \
    register uint64_t rsi asm ("rsi") = (uint64_t) (b); \
    register uint64_t rdx asm ("rdx") = (uint64_t) (c); \
    asm ("syscall": \
            "=r"(rax): \
            "r"(rax), \
            "r"(rdi), \
            "r"(rsi), \
            "r"(rdx)); \
    rax; \
    })

static inline ssize_t sc_write(int fd, const void *data, size_t count) {
    return SYSCALL3(1, fd, data, count);
}

static inline ssize_t sc_read(int fd, void *data, size_t count) {
    return SYSCALL3(0, fd, data, count);
}

void _start(void) {
    // sc_write(1, "Hello\n", 6);
    char c;

    while (sc_read(0, &c, 1) > 0) {
        if (c == '\n') {
            break;
        }
        sc_write(1, "Read a char: ", sizeof("Read a char: ") - 1);
        sc_write(1, &c, 1);
        sc_write(1, "\n", 1);
    }

    // TODO: sys_exit()
    while (1);
}
