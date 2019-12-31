#pragma once
#include <stdint.h>

struct thread;

struct wait_block {
    enum {
        WAIT_SLEEP,
    } type;
    union {
        uint64_t sleep_deadline;
    } data;

    struct wait_block *next;
};

void thread_nanosleep(struct thread *thr, uint64_t nano);
// TODO: interrupt threads
