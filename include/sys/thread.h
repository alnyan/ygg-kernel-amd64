#pragma once
#include "sys/types.h"
#include "sys/fs/vfs.h"

#define THREAD_KERNEL       (1 << 31)
#define THREAD_SIGRET       (1 << 29)
#define THREAD_CTX_SAVED    (1 << 28)
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
// Initialize I/O context and open stdio
#define THREAD_INIT_IOCTX   (1 << 1)
// Initialize platform context
#define THREAD_INIT_CTX     (1 << 0)

#if defined(ARCH_AMD64)
typedef uint64_t *mm_space_t;
#include "sys/amd64/sys/thread.h"
#endif

struct image_info {
    uintptr_t image_end;
    uintptr_t brk;
};

struct thread {
    __plat_thread data;

    struct image_info image;
    // Arguments are stored here
    uintptr_t argp_page;
    uintptr_t argp_page_phys;

    uint64_t flags;
    uint32_t pid;

    int exit_code;

    struct vfs_ioctx ioctx;
    struct ofile fds[4];

    // Signals
    uint32_t sigq;

    // Signal handler stack and entry
    uintptr_t sigentry;
    uintptr_t sigstack;

    mm_space_t space;

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
void thread_cleanup(struct thread *t);
void thread_terminate(struct thread *t);

void thread_signal(struct thread *t, int signum);
