#pragma once

#if defined(__ASM__)
.extern cpu

#if defined(AMD64_SMP)
#define get_cpu(off) \
    %gs:off
#else
#define get_cpu(off) \
    (__amd64_cpu + off)(%rip)
#endif

// To be used in interrupt handlers
// DIRECTLY AFTER ENTRY without prior stack modification
.macro swapgs_if_needed
    cmpq $0x08, 0x08(%rsp)
    je 1f
    swapgs
1:
.endm

#define CPU_SELF                0x00
#define CPU_THREAD              0x08
#define CPU_TSS                 0x10
#define CPU_SYSCALL_RSP         0x18

#define TSS_RSP0                0x04

#else
#include "sys/types.h"

struct cpu {
    // TODO: somehow export offsets to asm
    struct cpu *self;           // 0x00

    struct thread *thread;      // 0x08
    amd64_tss_t *tss;           // 0x10
    uint64_t syscall_rsp;       // 0x18
    uint64_t ticks;

    uint64_t processor_id;

    // No need to define offsets for these: ther're not accessed
    // from assembly
    uint64_t flags;
    uint64_t apic_id;
};
#endif
