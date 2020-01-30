#pragma once
#include "sys/amd64/asm/asm_thread.h"
#include "sys/mm.h"

struct thread {
    // Platform data
    struct thread_data data;

    uint64_t sleep_deadline;
    mm_space_t space;
    pid_t pid;

    struct thread *wait_prev, *wait_next;

    struct thread *prev, *next;
};

pid_t thread_alloc_pid(int is_user);

void thread_sleep(struct thread *thr, uint64_t deadline);
int thread_init(struct thread *thr, uintptr_t entry, void *arg, int user);
