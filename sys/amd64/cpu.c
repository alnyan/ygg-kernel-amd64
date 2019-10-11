#include "sys/amd64/cpu.h"

#if defined(AMD64_SMP)
struct cpu cpus[AMD64_MAX_SMP] = { 0 };
#else
struct cpu cpu;
#endif
