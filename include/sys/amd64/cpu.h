#pragma once
#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/types.h"
#include "sys/thread.h"

#define FXSAVE_REGION       512

struct cpu {
    // TODO: somehow export offsets to asm
    struct cpu *self;           // 0x00

    struct thread *thread;      // 0x08
    uint64_t ticks;             // 0x10
    amd64_tss_t *tss;           // 0x18
    uint64_t syscall_rsp;       // 0x20

    uint64_t processor_id;      // 0x28

    // No need to define offsets for these: ther're not accessed
    // from assembly
    uint64_t flags;
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

#define MSR_IA32_APIC_BASE          0x1B
#define IA32_APIC_BASE_BSP          0x100
#define IA32_APIC_BASE_ENABLE       0x800

#define MSR_IA32_EFER               0xC0000080
#define IA32_EFER_NXE               (1 << 11)

#define MSR_IA32_KERNEL_GS_BASE     0xC0000102

static inline uint64_t rdmsr(uint32_t addr) {
    uint64_t v;
    asm volatile ("rdmsr":"=A"(v):"c"(addr));
    return v;
}

static inline void wrmsr(uint32_t addr, uint64_t v) {
    uint32_t low = (v & 0xFFFFFFFF), high = v >> 32;
    asm volatile ("wrmsr"::"c"(addr),"a"(low),"d"(high));
}
