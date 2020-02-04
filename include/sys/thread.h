#pragma once
#if defined(ARCH_AMD64)
#include "arch/amd64/asm/asm_thread.h"
#endif
#include "fs/vfs.h"
#include "sys/mm.h"

#define THREAD_MAX_FDS          16

struct ofile;

enum thread_state {
    THREAD_READY = 1,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_WAITING_IO,
    THREAD_WAITING_PID,
    THREAD_STOPPED
};

#define THREAD_KERNEL           (1 << 0)
#define THREAD_EMPTY            (1 << 1)

#define thread_signal_clear(thr, signum) \
    (thr)->sigq &= ~(1ULL << ((signum) - 1))
#define thread_signal_set(thr, signum) \
    (thr)->sigq |= 1ULL << ((signum) - 1);
#define thread_signal_pending(thr, signum) \
    ((thr)->sigq & (1ULL << ((signum) - 1)))

struct thread {
    // Platform data and context
    struct thread_data data;
    mm_space_t space;
    size_t image_end;
    size_t brk;

    // I/O
    struct vfs_ioctx ioctx;
    struct ofile *fds[THREAD_MAX_FDS];

    // Wait
    uint64_t sleep_deadline;
    struct thread *wait_prev, *wait_next;

    // Signal
    uintptr_t signal_entry;
    uint64_t sigq;

    // State
    char name[256];
    uint32_t flags;
    pid_t pid;
    pid_t pgid;
    enum thread_state state;
    struct thread *parent;
    struct thread *first_child;
    struct thread *next_child;
    int exit_status;

    // Global thread list (for stuff like finding by PID)
    struct thread *g_prev, *g_next;

    // Scheduler
    int cpu;
    struct thread *prev, *next;
};

pid_t thread_alloc_pid(int is_user);
void thread_ioctx_fork(struct thread *dst, struct thread *src);
int thread_init(struct thread *thr, uintptr_t entry, void *arg, int user);
void thread_cleanup(struct thread *thr);

struct thread *thread_find(pid_t pid);
void thread_signal(struct thread *thr, int signum);
int thread_check_signal(struct thread *thr, int ret);
void thread_sigenter(int signum);
int thread_signal_pgid(pid_t pgid, int signum);

int thread_sleep(struct thread *thr, uint64_t deadline, uint64_t *int_time);
