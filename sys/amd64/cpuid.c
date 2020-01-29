#include "sys/amd64/cpuid.h"
#include "sys/panic.h"

uint32_t cpuid_features_edx, cpuid_features_ecx;
uint32_t cpuid_ext_features_edx, cpuid_ext_features_ecx;

void cpuid_init(void) {
    uint32_t buf[4];

    cpuid(CPUID_REQ_FEATURES, buf);

    cpuid_features_ecx = buf[1];
    cpuid_features_edx = buf[2];

    cpuid(CPUID_REQ_EXT_FEATURES, buf);

    cpuid_ext_features_ecx = buf[1];
    cpuid_ext_features_edx = buf[2];

    if (!(cpuid_ext_features_edx & CPUID_EXT_EDX_FEATURE_SYSCALL)) {
        panic("Support for SYSCALL instruction is required\n");
    }
}
