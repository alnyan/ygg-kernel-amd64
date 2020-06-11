#if defined(AMD64_SMP)
#include "arch/amd64/smp/ipi.h"
#include "arch/amd64/smp/smp.h"
#include "arch/amd64/cpu.h"
#endif
#include "sys/panic.h"
#include "sys/debug.h"

void panicf(const char *fmt, ...) {
    uintptr_t rbp;
    asm volatile ("movq %%rbp, %0":"=r"(rbp));

    asm volatile ("cli");
    va_list args;
    kfatal("--- Panic ---\n");

    va_start(args, fmt);
    debugs(DEBUG_FATAL, "\033[41m");
    debugfv(DEBUG_FATAL, fmt, args);
    debugs(DEBUG_FATAL, "\n\033[0m");
    va_end(args);

    kfatal("Call trace:\n");
    debug_backtrace(rbp, 0, 10);

    kfatal("--- Panic ---\n");

#if defined(AMD64_SMP)
    // Send PANIC IPIs to all other CPUs
    size_t cpu = get_cpu()->processor_id;
    kfatal("cpu%u initiates panic sequence\n", cpu);
    for (size_t i = 0; i < smp_ncpus; ++i) {
        if (i != cpu) {
            amd64_ipi_send(i, IPI_VECTOR_PANIC);
        }
    }
#endif

    panic_hlt();
}
