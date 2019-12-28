#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/debug.h"

extern void amd64_syscall_yield_stopped();

int amd64_pfault(uintptr_t cr2) {
    struct cpu *cpu = get_cpu();
    _assert(cpu);

    struct thread *thr = cpu->thread;
    kdebug("Page fault in %s thread %u\n",
        thr->flags & THREAD_KERNEL ? "kernel" : "user",
        thr->pid);

    kdebug("CR2 = %p\n", cr2);

    amd64_syscall_yield_stopped();
    return -1;
}

void amd64_pfault_kernel(uintptr_t rip, uintptr_t cr2) {
    kdebug("Kernel-space page fault:\n");
    kdebug("RIP = %p\n", rip);
    kdebug("CR2 = %p\n", cr2);
}
