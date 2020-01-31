#pragma once
#include "sys/amd64/asm/asm_thread.h"
#include "sys/fs/vfs.h"
#include "sys/mm.h"

#define THREAD_MAX_FDS          16

struct ofile;

enum thread_state {
    THREAD_READY = 1,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_WAITING_IO,
    THREAD_STOPPED
};

struct thread {
    // Platform data and context
    struct thread_data data;
    mm_space_t space;

    // I/O
    struct vfs_ioctx ioctx;
    struct ofile *fds[THREAD_MAX_FDS];

    // Wait
    uint64_t sleep_deadline;
    struct thread *wait_prev, *wait_next;

    // Signal
    uintptr_t signal_entry;

    // State
    pid_t pid;
    enum thread_state state;
    struct thread *parent;
    struct thread *first_child;
    struct thread *next_child;

    // Scheduler
    struct thread *prev, *next;
};

pid_t thread_alloc_pid(int is_user);

void thread_sleep(struct thread *thr, uint64_t deadline);
void thread_ioctx_fork(struct thread *dst, struct thread *src);
int thread_init(struct thread *thr, uintptr_t entry, void *arg, int user);
