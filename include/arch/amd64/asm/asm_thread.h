#pragma once

#define THREAD_RSP0             0x00
#define THREAD_RSP0_TOP         0x08
#define THREAD_CR3              0x10
#define THREAD_RSP0_SIGENTER    0x18

#if !defined(__ASM__)
#include "sys/types.h"

struct thread_data {
    uintptr_t rsp0;             // 0x00
    uintptr_t rsp0_top;         // 0x08
    uintptr_t cr3;              // 0x10
    uintptr_t rsp0_sigenter;    // 0x18

    uintptr_t rsp0_base, rsp0_size;
    uintptr_t rsp3_base, rsp3_size;

    void *fxsave;
};
#endif
