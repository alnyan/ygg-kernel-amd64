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
    kerror("RAX = %p, RCX = %p\n", ctx->rax, ctx->rcx);
    kerror("RDX = %p, RBX = %p\n", ctx->rdx, ctx->rbx);
    kerror("RSP = %p, RBP = %p\n", ctx->rsp, ctx->rbp);
    kerror("RSI = %p, RDI = %p\n", ctx->rsi, ctx->rdi);
    kerror(" R8 = %p,  R9 = %p\n", ctx->r8,  ctx->r9);
    kerror("R10 = %p, R11 = %p\n", ctx->r10, ctx->r11);
    kerror("R12 = %p, R13 = %p\n", ctx->r12, ctx->r13);
    kerror("R14 = %p, R15 = %p\n", ctx->r12, ctx->r15);

    // Can't access IRET-registers (CS/RIP and the likes)
    // because they're presumably fucked up by error code
    // push

    amd64_syscall_yield_stopped();
    return -1;
}

void amd64_pfault_kernel(uintptr_t rip, uintptr_t cr2) {
    kfatal("Kernel-space page fault:\n");
    kfatal("RIP = %p\n", rip);
    kfatal("CR2 = %p\n", cr2);
}
