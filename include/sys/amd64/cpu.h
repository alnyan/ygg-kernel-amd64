#pragma once

struct cpu {
    struct cpu *self;
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
