#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/debug.h"

extern void amd64_syscall_yield_stopped();

int amd64_pfault(uintptr_t cr2) {
    // TODO: would be nice to get error code off the stack and check what kind of
    // error happened
    struct cpu *cpu = get_cpu();
    _assert(cpu);

    struct thread *thr = cpu->thread;
    _assert(thr);

    struct cpu_context *ctx = (struct cpu_context *) thr->data.rsp0;

    kerror("Page fault in %s thread %u\n",
        thr->flags & THREAD_KERNEL ? "kernel" : "user",
        thr->pid);

    kerror("CR2 = %p\n", cr2);

    // Dump registers
    cpu_print_context(DEBUG_ERROR, ctx);

    // Can't access IRET-registers (CS/RIP and the likes)
    // because they're presumably fucked up by error code
    // push

    thr->flags |= THREAD_STOPPED;
    amd64_syscall_yield_stopped();
    return -1;
}

void amd64_pfault_kernel(uintptr_t rip, uintptr_t cr2) {
    kfatal("Kernel-space page fault:\n");
    kfatal("RIP = %p\n", rip);
    kfatal("CR2 = %p\n", cr2);
}
