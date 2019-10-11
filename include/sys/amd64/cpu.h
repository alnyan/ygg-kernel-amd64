#pragma once

struct cpu {
    // TODO: somehow export offsets to asm
    struct cpu *self;           // 0x00
    uint64_t flags;             // 0x08

    uint64_t processor_id;      // 0x10
    uint64_t apic_id;           // 0x18

    uint64_t ticks;             // 0x20
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
