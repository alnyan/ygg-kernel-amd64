#pragma once
#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/types.h"

#define FXSAVE_REGION       512

#include "sys/amd64/asm/asm_cpu.h"
#define thread_self         get_cpu()->thread

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
