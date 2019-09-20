#include "sys/thread.h"
#include "sys/debug.h"

/**
 * @brief Interrupt-based syscall entry
 */
void amd64_syscall_int(void) {
    // TODO: write something meaningful here
    uint64_t cnt = 1000000;

    kdebug("Waiting\n");
    while (cnt != 1) {
        asm volatile ("sti");
        --cnt;
    }
    kdebug("Waking up\n");
}
