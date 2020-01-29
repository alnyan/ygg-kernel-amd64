#include "sys/amd64/mm/map.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/signum.h"
#include "sys/debug.h"

extern void amd64_syscall_yield_stopped();

int amd64_pfault(uintptr_t cr2) {
}

void amd64_pfault_kernel(uintptr_t rip, uintptr_t cr2) {
    kfatal("Kernel-space page fault:\n");
    kfatal("RIP = %p\n", rip);
    kfatal("CR2 = %p\n", cr2);
}
