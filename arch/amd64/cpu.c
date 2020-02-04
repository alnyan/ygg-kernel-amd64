#include "arch/amd64/cpu.h"
#include "sys/debug.h"

#if defined(AMD64_SMP)
struct cpu cpus[AMD64_MAX_SMP] = { 0 };
#else
struct cpu __amd64_cpu;
#endif
