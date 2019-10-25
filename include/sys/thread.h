#pragma once
#include "sys/types.h"
#include "sys/fs/vfs.h"

#define THREAD_KERNEL       (1 << 31)

#if defined(ARCH_AMD64)
typedef uint64_t *mm_space_t;
#include "sys/amd64/sys/thread.h"
#endif

struct thread {
    __plat_thread data;

    uint64_t flags;
    uint32_t pid;
    uint32_t parent_pid;

    struct vfs_ioctx ioctx;
    struct ofile fds[4];

    mm_space_t space;

    // TODO: maybe __sched_thread
    struct thread *next;
};

int thread_init(struct thread *t,
                mm_space_t space,
                uintptr_t entry,
                uintptr_t stack0_base,
                size_t stack0_size,
                uintptr_t stack3_base,
                size_t stack3_size,
                uint32_t flags,
                void *arg);
