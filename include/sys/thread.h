#pragma once
#include "sys/amd64/asm/asm_thread.h"

struct thread {
    // Platform data
    struct thread_data data;

    pid_t pid;

    struct thread *prev, *next;
};

pid_t thread_alloc_pid(int is_user);

int thread_init(struct thread *thr, uintptr_t entry, void *arg, int user);
