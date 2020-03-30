#pragma once
#include "sys/types.h"
#include "sys/spin.h"
#include "sys/list.h"

struct io_notify {
    spin_t lock;
    struct thread *owner;
    size_t value;
    struct list_head link, own_link;
};

// Multiple-notifier wait
void thread_wait_io_add(struct thread *t, struct io_notify *n);
int thread_wait_io_any(struct thread *t, struct io_notify **r_n);
void thread_wait_io_clear(struct thread *t);

// Wait for single specific I/O notification
int thread_wait_io(struct thread *t, struct io_notify *n);
void thread_notify_io(struct io_notify *n);

void thread_wait_io_init(struct io_notify *n);
