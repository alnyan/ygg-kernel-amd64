#pragma once
#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/types.h"
#include "sys/thread.h"

struct cpu {
    // TODO: somehow export offsets to asm
    struct cpu *self;           // 0x00

    struct thread *thread;      // 0x08
    uint64_t ticks;             // 0x10
    amd64_tss_t *tss;           // 0x18
    uint64_t syscall_rsp;       // 0x20

    // No need to define offsets for these: ther're not accessed
    // from assembly
    uint64_t flags;

    uint64_t processor_id;
    uint64_t apic_id;
};

struct cpu_context {
#if defined(AMD64_STACK_CTX_CANARY)
    uintptr_t __canary;
#endif
    uintptr_t fs, es, ds;
    uintptr_t cr3;
    uintptr_t r15, r14, r13, r12;
    uintptr_t r11, r10, r9, r8;
    uintptr_t rdi, rsi, rbp;
    uintptr_t rbx, rdx, rcx, rax;
    uintptr_t rip, cs, rflags, rsp, ss;
};

#if defined(AMD64_SMP)
extern struct cpu cpus[AMD64_MAX_SMP];

static inline struct cpu *get_cpu(void) {
    struct cpu *cpu;
    asm volatile ("movq %%gs:0, %0":"=r"(cpu));
    return cpu;
}
#else
extern struct cpu __amd64_cpu;

#define get_cpu()   ((struct cpu *) &__amd64_cpu)
#endif

void cpu_print_context(int level, struct cpu_context *ctx);
