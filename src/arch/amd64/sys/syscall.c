#include "sys/thread.h"
#include "sys/debug.h"

/**
 * @brief Interrupt-based syscall entry
 */
void amd64_syscall_int(void) {
    // TODO: write something meaningful here
    uint64_t cnt = 1000000;

    extern uintptr_t amd64_thread_current;
    kdebug("Waiting %p\n", amd64_thread_current);
    while (cnt != 1) {
        asm volatile ("sti");
        --cnt;
    }
    asm volatile ("cli");
    kdebug("Waking up\n");
}
