#pragma once
#include "sys/types.h"

#define __plat_thread struct amd64_thread

struct thread;
struct amd64_thread {
    uintptr_t rsp0, rsp0s;
    // Normal context
    uintptr_t stack0_base;
    uintptr_t stack0_size;

    // Signal handling context
    uintptr_t stack0s_base;
    uintptr_t stack0s_size;

    // User stack
    uintptr_t stack3_base;
    uintptr_t stack3_size;

    uint32_t data_flags;
};

void amd64_thread_set_ip(struct thread *t, uintptr_t ip);
void amd64_thread_sigenter(struct thread *t, int signum);
void amd64_thread_sigret(struct thread *t);
