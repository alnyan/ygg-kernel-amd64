#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/kidle.h"
#include "sys/debug.h"
#include "sys/time.h"

static char kidle_kstack[8192];
static thread_t kidle_thread;

static void kidle(void) {
    while (1) {
        asm volatile ("sti; hlt");
    }
}

void kidle_init(void) {
    assert(thread_init(&kidle_thread,
                       mm_kernel,
                       (uintptr_t) kidle,
                       (uintptr_t) kidle_kstack,
                       sizeof(kidle_kstack),
                       0,
                       0,
                       THREAD_KERNEL) == 0,
           "Failed to set kidle up\n");
    sched_add(&kidle_thread);
}
