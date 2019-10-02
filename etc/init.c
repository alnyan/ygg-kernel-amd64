#include <sys/types.h>

static inline void sc_write(int fd, const void *data, size_t count) {
    asm volatile ("syscall"::"rax"(0x00),"rdi"(fd),"rsi"(1),"rdx"(count));
}

void _start(void) {
    sc_write(1, "Hello\n", 6);
    while (1);
}
