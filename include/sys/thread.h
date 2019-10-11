#pragma once
#include "sys/mm.h"

#if defined(ARCH_AMD64)
#include "sys/amd64/sys/thread.h"
#endif

struct thread {
    __plat_thread data;

    uint64_t flags;
    uint32_t pid;
    uint32_t parent_pid;

    mm_space_t space;
};
