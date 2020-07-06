#pragma once
#if defined(ARCH_AMD64)
#include "arch/amd64/asm/asm_thread.h"
#endif
#include "sys/wait.h"
#include "sys/list.h"
#include "fs/vfs.h"
#include "sys/mm.h"

#define THREAD_MAX_FDS          16

struct ofile;

enum thread_state {
    THREAD_READY = 1,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_STOPPED
};

enum process_state {
    PROC_ACTIVE,
    PROC_SUSPENDED,
    PROC_FINISHED
};

#define THREAD_KERNEL           (1 << 0)
#define PROC_EMPTY              (1 << 1)
#define THREAD_FPU_SAVED        (1 << 2)
#define THREAD_IDLE             (1 << 3)

#define process_signal_clear(thr, signum) \
    (thr)->sigq &= ~(1ULL << ((signum) - 1))
#define process_signal_set(thr, signum) \
    (thr)->sigq |= 1ULL << ((signum) - 1);
#define process_signal_pending(thr, signum) \
    ((thr)->sigq & (1ULL << ((signum) - 1)))

struct process;

struct thread {
    // Platform data and context
    struct thread_data data;
    enum thread_state state;

    // Wait
    uint64_t sleep_deadline;
    struct list_head wait_head;
    struct io_notify sleep_notify;

    struct process *proc;
    struct thread *next_thread;

    uint32_t flags;

    // Scheduler
    int cpu;
    struct thread *sched_prev, *sched_next;
};

struct process {
    mm_space_t space;
    size_t image_end;
    size_t brk;

    int proc_state;

    // Threads
    struct thread *first_thread;
    size_t thread_count;

    // Shared memory
    struct list_head shm_list;

    // I/O
    struct vfs_ioctx ioctx;
    struct ofile *fds[THREAD_MAX_FDS];

    // Wait
    struct io_notify pid_notify;

    // Signal
    uintptr_t signal_entry;
    uint64_t sigq;

    // State
    char name[256];
    uint32_t flags;
    pid_t pid;
    pid_t pgid;
    struct process *parent;
    struct process *first_child;
    struct process *next_child;
    int exit_status;

    // Global process list (for stuff like finding by PID)
    struct list_head g_link;
};

pid_t process_alloc_pid(int is_user);

int process_init_thread(struct process *proc, uintptr_t entry, void *arg, int user);

//pid_t thread_alloc_pid(int is_user);
//void thread_ioctx_fork(struct thread *dst, struct thread *src);
#define THR_INIT_USER           (1 << 0)
#define THR_INIT_STACK_SET      (1 << 1)
int thread_init(struct thread *thr, uintptr_t entry, void *arg, int flags);
//void thread_cleanup(struct thread *thr);

struct process *process_child(struct process *of, pid_t pid);
void process_unchild(struct process *proc);
void process_free(struct process *proc);

struct process *process_find(pid_t pid);
void process_signal(struct process *proc, int signum);
int thread_check_signal(struct thread *thr, int ret);
void thread_sigenter(int signum);
int process_signal_pgid(pid_t pgid, int signum);

int thread_sleep(struct thread *thr, uint64_t deadline, uint64_t *int_time);
