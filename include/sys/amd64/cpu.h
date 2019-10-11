#pragma once
#include "sys/types.h"

struct cpu {
    // TODO: somehow export offsets to asm
    struct cpu *self;           // 0x00

    void *thread;               // 0x08
    uint64_t ticks;             // 0x10

    // No need to define offsets for these: ther're not accessed
    // from assembly
    uint64_t flags;

    uint64_t processor_id;
    uint64_t apic_id;
};

#if defined(AMD64_SMP)
extern struct cpu cpus[AMD64_MAX_SMP];

static inline struct cpu *get_cpu(void) {
    struct cpu *cpu;
    asm volatile ("movq %%gs:0, %0":"=r"(cpu));
    return cpu;
}
#else
extern struct cpu cpu;

#define get_cpu()   &cpu
#endif
