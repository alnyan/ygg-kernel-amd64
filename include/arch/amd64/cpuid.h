#pragma once
#include "sys/types.h"

#define CPUID_REQ_VENDOR                0x00
#define CPUID_REQ_FEATURES              0x01
#define CPUID_REQ_CACHE                 0x02
#define CPUID_REQ_SERIAL                0x03
#define CPUID_REQ_EXT_FEATURES          0x80000001

#define CPUID_EDX_FEATURE_PAT           (1U << 16)
#define CPUID_EDX_FEATURE_MTRR          (1U << 12)

#define CPUID_EXT_EDX_FEATURE_NX        (1U << 20)
#define CPUID_EXT_EDX_FEATURE_SYSCALL   (1U << 11)

extern uint32_t cpuid_features_ecx, cpuid_features_edx;
extern uint32_t cpuid_ext_features_ecx, cpuid_ext_features_edx;

static inline void cpuid(uint32_t eax, uint32_t *out) {
    asm volatile ("cpuid":"=a"(out[0]),"=c"(out[1]),"=d"(out[2]),"=b"(out[3]):"a"(eax));
}

void cpuid_init(void);
