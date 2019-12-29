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

#endif
