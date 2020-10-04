#if defined(AMD64_SMP)
#include "arch/amd64/smp/ipi.h"
#include "arch/amd64/smp/smp.h"
#endif
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/panic.h"
#include "sys/debug.h"

void panicf(const char *fmt, ...) {
    uintptr_t rbp;
    asm volatile ("movq %%rbp, %0":"=r"(rbp));

    asm volatile ("cli");
    va_list args;
    kfatal("--- Panic (cpu%d) ---\n", get_cpu()->processor_id);

    if (sched_ready) {
        debugs(DEBUG_FATAL, "\033[41m");
        thread_dump(DEBUG_FATAL, thread_self);
        debugs(DEBUG_FATAL, "\033[0m");
    }

    va_start(args, fmt);
    debugs(DEBUG_FATAL, "\033[41m");
    debugfv(DEBUG_FATAL, fmt, args);
    va_end(args);

    debugs(DEBUG_FATAL, "Call trace:\n");
    debug_backtrace(DEBUG_FATAL, rbp, 0, 10);
    debugs(DEBUG_FATAL, "\033[0,");

    kfatal("--- Panic ---\n");

#if defined(AMD64_SMP)
    // Send PANIC IPIs to all other CPUs
    size_t cpu = get_cpu()->processor_id;
    for (size_t i = 0; i < smp_ncpus; ++i) {
        if (i != cpu) {
            amd64_ipi_send(i, IPI_VECTOR_PANIC);
        }
    }
#endif

    panic_hlt();
}
