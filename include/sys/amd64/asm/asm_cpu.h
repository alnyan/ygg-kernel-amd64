#pragma once

#if defined(__ASM__)
.extern cpu

#if defined(AMD64_SMP)
#define get_cpu(off) \
    %gs:off
#else
#define get_cpu(off) \
    (cpu + off)(%rip)
#endif
#endif
