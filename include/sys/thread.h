#pragma once
#include "sys/types.h"
#include "sys/fs/vfs.h"

#define THREAD_KERNEL       (1 << 31)
#define THREAD_SIGRET       (1 << 29)
#define THREAD_CTX_SAVED    (1 << 28)

// Terminal ^D for this task
#define THREAD_EOF          (1 << 7)
#define THREAD_INTERRUPTED  (1 << 6)
// Thread is stopped, wait()ed by its parent and ready for reaping
#define THREAD_DONE_WAITING (1 << 5)
// Well, this is actually the opposite - not a child waiting to be
// wait()ed by a parent (as I have no wait() yet), but rather a
// parent awaiting child process tree termination.
// XXX: RENAME THIS TO SOMETHING IN LATER COMMITS
#define THREAD_ZOMBIE       (1 << 4)
#define THREAD_WAITING      (1 << 3)
#define THREAD_STOPPED      (1 << 2)

// Thread init flags:
// Initialize platform context
#define THREAD_INIT_CTX     (1 << 0)

#if defined(ARCH_AMD64)
typedef uint64_t *mm_space_t;
#include "sys/amd64/sys/thread.h"
#endif

#define THREAD_MAX_FDS      16

struct image_info {
    uintptr_t image_end;
    uintptr_t brk;
};

struct thread_times {
    // Last time scheduler selected this task
    uint64_t last_schedule_time;
    // Total amount of time spent in this task
    uint64_t time_spent;
    uint64_t last_io_wait;
    uint64_t time_io;
};

struct thread {
    __plat_thread data;

    char name[32];

    struct image_info image;

    uint64_t flags;
    pid_t pid;
    pid_t pgid;
    pid_t sid;

    int exit_code;

    struct vfs_ioctx ioctx;
    struct ofile *fds[THREAD_MAX_FDS];

    // Signals
    uint32_t sigq;

    // Signal handler stack and entry
    uintptr_t sigentry;
    uintptr_t sigstack;

    mm_space_t space;

    // Timing info
    struct thread_times times;

    // TODO: maybe __sched_thread
    int cpu;
    struct thread *prev;
    struct thread *next;

    struct thread *parent;
    struct thread *child;
    struct thread *next_child;
};

int thread_init(struct thread *t,
                mm_space_t space,
                uintptr_t entry,
                uintptr_t stack0_base,
                size_t stack0_size,
                uintptr_t stack3_base,
                size_t stack3_size,
                uint32_t flags,
                uint32_t init_flags,
                void *arg);
int thread_fork(struct thread *dst, struct thread *src);

void thread_set_name(struct thread *thr, const char *name);

void thread_cleanup(struct thread *t);
void thread_terminate(struct thread *t);

void thread_signal(struct thread *t, int signum);

void thread_ioctx_fork(struct thread *dst, struct thread *src);
void thread_ioctx_cleanup(struct thread *t);
