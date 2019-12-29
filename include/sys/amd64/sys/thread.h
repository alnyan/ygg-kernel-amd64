#pragma once
#include "sys/types.h"

#define __plat_thread struct amd64_thread

struct thread;
struct amd64_thread {
    uintptr_t rsp0;
    uintptr_t stack0_base;
    uintptr_t stack0_size;

    uintptr_t stack3_base;
    uintptr_t stack3_size;

    uint32_t data_flags;
};

void amd64_thread_set_ip(struct thread *t, uintptr_t ip);
