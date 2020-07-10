#include "arch/amd64/mm/pool.h"
#include "arch/amd64/context.h"
#include "sys/mem/vmalloc.h"
#include "arch/amd64/cpu.h"
#include "sys/binfmt_elf.h"
#include "sys/sys_proc.h"
#include "sys/mem/phys.h"
#include "user/signum.h"
#include "user/errno.h"
#include "user/fcntl.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"

struct sys_fork_frame {
    uint64_t rdi, rsi, rdx, rcx;
    uint64_t r8, r9, r10, r11;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rflags;
    uint64_t rip;
};

////

LIST_HEAD(proc_all_head);
static pid_t last_kernel_pid = 0;
static pid_t last_user_pid = 0;
// TODO: MAKE THIS PER-PROCESSOR
static uint64_t fxsave_buf[FXSAVE_REGION / 8] __attribute__((aligned(16)));

void context_save_fpu(struct thread *new, struct thread *old) {
    _assert(old);
    if (old->data.fxsave) {
        asm volatile ("fxsave (%0)"::"r"(fxsave_buf):"memory");
        memcpy(old->data.fxsave, fxsave_buf, FXSAVE_REGION);
        old->flags |= THREAD_FPU_SAVED;
    }

}

void context_restore_fpu(struct thread *new, struct thread *old) {
    _assert(new);
    if (new->flags & THREAD_FPU_SAVED) {
        memcpy(fxsave_buf, new->data.fxsave, FXSAVE_REGION);
        asm volatile ("fxrstor (%0)"::"r"(fxsave_buf):"memory");
        new->flags &= ~THREAD_FPU_SAVED;
    }
}

pid_t process_alloc_pid(int is_user) {
    if (is_user) {
        return ++last_user_pid;
    } else {
        return -(++last_kernel_pid);
    }
}



static void process_ioctx_empty(struct process *proc) {
    memset(&proc->ioctx, 0, sizeof(struct vfs_ioctx));
    memset(proc->fds, 0, sizeof(proc->fds));
}

void process_ioctx_fork(struct process *dst, struct process *src) {
    process_ioctx_empty(dst);

    dst->ioctx.cwd_vnode = src->ioctx.cwd_vnode;
    dst->ioctx.gid = src->ioctx.gid;
    dst->ioctx.uid = src->ioctx.uid;

    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (src->fds[i]) {
            dst->fds[i] = ofile_dup(src->fds[i]);
        }
    }
}



int process_signal_pgid(pid_t pgid, int signum) {
    int ret = 0;

    struct process *proc;
    list_for_each_entry(proc, &proc_all_head, g_link) {
        if (proc->proc_state != PROC_FINISHED && proc->pgid == pgid) {
            process_signal(proc, signum);
            ++ret;
        }
    }

    return ret == 0 ? -1 : ret;
}

struct process *process_find(pid_t pid) {
    struct process *proc;
    list_for_each_entry(proc, &proc_all_head, g_link) {
        if (proc->pid == pid) {
            return proc;
        }
    }

    return NULL;
}

struct thread *process_first_thread(struct process *proc) {
    _assert(proc);
    _assert(!list_empty(&proc->thread_list) && proc->thread_count);
    return list_first_entry(&proc->thread_list, struct thread, thread_link);
}

struct process *process_child(struct process *of, pid_t pid) {
    for (struct process *proc = of->first_child; proc; proc = proc->next_child) {
        if (proc->pid == pid) {
            return proc;
        }
    }
    return NULL;
}

void process_unchild(struct process *proc) {
    struct process *par = proc->parent;
    _assert(par);

    struct process *p = NULL;
    struct process *c = par->first_child;
    int found = 0;

    while (c) {
        if (c == proc) {
            found = 1;
            if (p) {
                p->next_child = proc->next_child;
            } else {
                par->first_child = proc->next_child;
            }
            break;
        }

        p = c;
        c = c->next_child;
    }

    _assert(found);
}

void process_cleanup(struct process *proc) {
    // Leave only the system context required for hierachy tracking and error code/pid
    _assert(proc->proc_state == PROC_FINISHED);
    _assert(proc->thread_count == 1);
    proc->flags |= PROC_EMPTY;
    kdebug("Cleaning up %d\n", proc->pid);
    for (size_t i = 0; i < THREAD_MAX_FDS; ++i) {
        if (proc->fds[i]) {
            ofile_close(&proc->ioctx, proc->fds[i]);
            proc->fds[i] = NULL;
        }
    }

    // Release userspace pages
    mm_space_release(proc);
}

void process_free(struct process *proc) {
    // Make sure all the threads of the process have stopped -
    // only main remains
    _assert(proc->thread_count == 1);

    // Sure that no code of this thread will be running anymore -
    // can clean up its stuff
    process_cleanup(proc);

    _assert(proc->proc_state == PROC_FINISHED);
    struct thread *thr;
    thr = list_first_entry(&proc->thread_list, struct thread, thread_link);
    _assert(thr);

    // Free kstack
    for (size_t i = 0; i < thr->data.rsp0_size / MM_PAGE_SIZE; ++i) {
        mm_phys_free_page(MM_PHYS(i * MM_PAGE_SIZE + thr->data.rsp0_base));
    }

    // Free page directory (if not mm_kernel)
    if (proc->space != mm_kernel) {
        // Make sure we don't shoot a leg off
        uintptr_t cr3;
        asm volatile ("movq %%cr3, %0":"=a"(cr3));
        _assert(MM_VIRTUALIZE(cr3) != (uintptr_t) proc->space);

        mm_space_free(proc);
    }

    kfree(thr->data.fxsave);

    // Free thread itself
    memset(thr, 0, sizeof(struct thread));
    kfree(thr);
    // Free the process
    memset(proc, 0, sizeof(struct process));
    kfree(proc);
}

int process_init_thread(struct process *proc, uintptr_t entry, void *arg, int user) {
    list_head_init(&proc->g_link);
    list_head_init(&proc->shm_list);
    thread_wait_io_init(&proc->pid_notify);

    proc->name[0] = 0;
    proc->flags = user ? 0 : THREAD_KERNEL;

    if (user) {
        proc->space = amd64_mm_pool_alloc();
        mm_space_clone(proc->space, mm_kernel, MM_CLONE_FLG_KERNEL);
    } else {
        proc->space = mm_kernel;
    }

    struct thread *main_thread = kmalloc(sizeof(struct thread));
    _assert(main_thread);
    main_thread->proc = proc;
    list_head_init(&proc->thread_list);

    int res = thread_init(main_thread, entry, arg, user ? THR_INIT_USER : 0);
    _assert(res == 0);

    process_ioctx_empty(proc);

    list_add(&main_thread->thread_link, &proc->thread_list);
    proc->thread_count = 1;
    proc->parent = NULL;
    proc->first_child = NULL;
    proc->next_child = NULL;
    proc->pgid = -1;
    proc->pid = process_alloc_pid(user);
    kdebug("New process #%d with main thread <%p>", proc->pid, main_thread);

    proc->sigq = 0;
    proc->proc_state = PROC_ACTIVE;

    list_add(&proc->g_link, &proc_all_head);

    return 0;
}

int sys_clone(int (*fn) (void *), void *stack, int flags, void *arg) {
    struct process *proc = thread_self->proc;
    _assert(proc);
    struct thread *thr = kmalloc(sizeof(struct thread));
    _assert(thr);

    memset(thr, 0, sizeof(struct thread));
    thr->proc = proc;
    // XXX: Hacky
    thr->data.rsp3_base = (uintptr_t) stack;
    thr->data.rsp3_size = 0;

    int res = thread_init(thr, (uintptr_t) fn, arg, THR_INIT_USER | THR_INIT_STACK_SET);

    if (res < 0) {
        return res;
    }

    thr->signal_entry = thread_self->signal_entry;

    list_add(&thr->thread_link, &proc->thread_list);
    ++proc->thread_count;

    sched_queue(thr);

    return 0;
}

int thread_init(struct thread *thr, uintptr_t entry, void *arg, int flags) {
    uintptr_t stack_pages = mm_phys_alloc_contiguous(2); //amd64_phys_alloc_contiguous(2);
    _assert(stack_pages != MM_NADDR);

    thr->signal_entry = MM_NADDR;
    thr->signal_stack_base = MM_NADDR;
    thr->signal_stack_size = 0;

    thr->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    thr->data.rsp0_size = MM_PAGE_SIZE * 2;
    thr->data.rsp0_top = thr->data.rsp0_base + thr->data.rsp0_size;
    thr->flags = (flags & THR_INIT_USER) ? 0 : THREAD_KERNEL;
    list_head_init(&thr->thread_link);

    if (flags & THR_INIT_USER) {
        thr->data.fxsave = kmalloc(FXSAVE_REGION);
        _assert(thr->data.fxsave);
    } else {
        thr->data.fxsave = NULL;
    }

    list_head_init(&thr->wait_head);
    thread_wait_io_init(&thr->sleep_notify);

    uint64_t *stack = (uint64_t *) (thr->data.rsp0_base + thr->data.rsp0_size);

    mm_space_t space = NULL;
    if (thr->proc) {
        space = thr->proc->space;
    } else if (!(flags & THR_INIT_USER)) {
        space = mm_kernel;
    }
    _assert(space);
    thr->data.cr3 = MM_PHYS(space);

    if (flags & THR_INIT_USER) {
        uintptr_t ustack_base;
        if (!(flags & THR_INIT_STACK_SET)) {
            // Allocate thread user stack
            ustack_base = vmalloc(space, 0x1000000, 0xF0000000, 4, MM_PAGE_WRITE | MM_PAGE_USER, PU_PRIVATE);
            thr->data.rsp3_base = ustack_base;
            thr->data.rsp3_size = MM_PAGE_SIZE * 4;
        }
    }

    thr->state = THREAD_READY;

    // Initial thread context
    // Entry context
    if (flags & THR_INIT_USER) {
        // ss
        *--stack = 0x1B;
        // rsp
        *--stack = thr->data.rsp3_base + thr->data.rsp3_size;
        // rflags
        *--stack = 0x200;
        // cs
        *--stack = 0x23;
        // rip
        *--stack = (uintptr_t) entry;
    } else {
        // ss
        *--stack = 0x10;
        // rsp. Once this context is popped from the stack, stack top is going to be a new
        //      stack pointer for kernel threads
        *--stack = thr->data.rsp0_base + thr->data.rsp0_size;
        // rflags
        *--stack = 0x200;
        // cs
        *--stack = 0x08;
        // rip
        *--stack = (uintptr_t) entry;
    }

    // Caller-saved
    // r11
    *--stack = 0;
    // r10
    *--stack = 0;
    // r9
    *--stack = 0;
    // r8
    *--stack = 0;
    // rcx
    *--stack = 0;
    // rdx
    *--stack = 0;
    // rsi
    *--stack = 0;
    // rdi
    *--stack = (uintptr_t) arg;
    // rax
    *--stack = 0;

    // Small stub so that context switch enters the thread properly
    *--stack = (uintptr_t) context_enter;
    // Callee-saved
    // r15
    *--stack = 0;
    // r14
    *--stack = 0;
    // r13
    *--stack = 0;
    // r12
    *--stack = 0;
    // rbp
    *--stack = 0;
    // rbx
    *--stack = 0;

    // Thread lifecycle:
    // * context_switch_to():
    //   - pops callee-saved registers (initializing them to 0)
    //   - enters context_enter()
    // * context_enter():
    //   - pops caller-saved registers (initializing them to 0 and setting up rdi)
    //   - enters proper execution context via iret
    //  ... Thread is running here until it yields
    // * yield leads to context_switch_to():
    //   - call to yield() automatically (per ABI) stores caller-saved registers
    //   - context_switch_to() pushes callee-saved registers onto current stack
    //   - selects a new thread
    //   - step one

    thr->data.rsp0 = (uintptr_t) stack;

    return 0;
}

// TODO: support kthread forking()
//       (Although I don't really think it's very useful -
//        threads can just be created by thread_init() and
//        sched_queue())
int sys_fork(struct sys_fork_frame *frame) {
    struct thread *src_thread = thread_self;
    _assert(src_thread);
    struct process *src = src_thread->proc;
    _assert(src);

    if (src->thread_count != 1) {
        panic("XXX: fork() a multithreaded process\n");
    }

    struct process *dst = kmalloc(sizeof(struct process));
    _assert(dst);
    list_head_init(&dst->thread_list);
    struct thread *dst_thread = kmalloc(sizeof(struct thread));
    _assert(dst_thread);
    list_head_init(&dst_thread->thread_link);

    // Tie the two together
    list_add(&dst_thread->thread_link, &dst->thread_list);
    dst->thread_count = 1;
    dst_thread->proc = dst;

    // Initialize dst process: memory space
    mm_space_t space = amd64_mm_pool_alloc();
    dst->space = space;
    mm_space_fork(dst, src, MM_CLONE_FLG_KERNEL | MM_CLONE_FLG_USER);

    // Initialize dst process state
    list_head_init(&dst->g_link);
    list_head_init(&dst->shm_list);
    thread_wait_io_init(&dst->pid_notify);
    dst->flags = 0;
    strcpy(dst->name, src->name);
    process_ioctx_fork(dst, src);
    dst->parent = src;
    dst->next_child = src->first_child;
    src->first_child = dst;
    dst->first_child = NULL;
    dst->pid = process_alloc_pid(1);
    dst->pgid = src->pgid;
    dst->sigq = 0;
    dst->proc_state = PROC_ACTIVE;
    kdebug("New process #%d with main thread <%p>\n", dst->pid, dst_thread);

    // Initialize dst thread
    uintptr_t stack_pages = mm_phys_alloc_contiguous(2); //amd64_phys_alloc_contiguous(2);
    _assert(stack_pages != MM_NADDR);
    list_head_init(&dst_thread->wait_head);
    thread_wait_io_init(&dst_thread->sleep_notify);

    dst_thread->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    dst_thread->data.rsp0_size = MM_PAGE_SIZE * 2;
    dst_thread->data.rsp0_top = dst_thread->data.rsp0_base + dst_thread->data.rsp0_size;
    dst_thread->flags = 0;

    dst_thread->data.rsp3_base = src_thread->data.rsp3_base;
    dst_thread->data.rsp3_size = src_thread->data.rsp3_size;

    dst_thread->signal_entry = src_thread->signal_entry;
    dst_thread->signal_stack_base = src_thread->signal_stack_base;
    dst_thread->signal_stack_size = src_thread->signal_stack_size;

    dst_thread->data.cr3 = MM_PHYS(space);

    dst_thread->data.fxsave = kmalloc(FXSAVE_REGION);
    _assert(dst_thread->data.fxsave);
    _assert(src_thread->data.fxsave);

    if (src_thread->flags & THREAD_FPU_SAVED) {
        memcpy(dst_thread->data.fxsave, src_thread->data.fxsave, FXSAVE_REGION);
    }

    dst_thread->state = THREAD_READY;

    uint64_t *stack = (uint64_t *) dst_thread->data.rsp0_top;

    // Initial thread context
    // Entry context
    // ss
    *--stack = 0x1B;
    // rsp
    *--stack = frame->rsp;
    // rflags
    _assert(frame->rflags & 0x200);
    *--stack = frame->rflags;
    // cs
    *--stack = 0x23;
    // rip
    *--stack = frame->rip;

    // Caller-saved
    // r11
    *--stack = frame->r11;
    // r10
    *--stack = frame->r10;
    // r9
    *--stack = frame->r9;
    // r8
    *--stack = frame->r8;
    // rcx
    *--stack = frame->rcx;
    // rdx
    *--stack = frame->rdx;
    // rsi
    *--stack = frame->rsi;
    // rdi
    *--stack = frame->rdi;
    // rax
    *--stack = 0;

    // Small stub so that context switch enters the thread properly
    *--stack = (uintptr_t) context_enter;
    // Callee-saved
    // r15
    *--stack = frame->r15;
    // r14
    *--stack = frame->r14;
    // r13
    *--stack = frame->r13;
    // r12
    *--stack = frame->r12;
    // rbp
    *--stack = frame->rbp;
    // rbx
    *--stack = frame->rbx;

    dst_thread->data.rsp0 = (uintptr_t) stack;

    list_add(&dst->g_link, &proc_all_head);
    sched_queue(dst_thread);

    return dst->pid;
}

void thread_sigenter(int signum) {
    struct thread *thr = thread_self;
    struct process *proc = thr->proc;
    if (signum == SIGCHLD || signum == SIGCONT) {
        return;
    }
    kdebug("%d: Handle signal %d\n", proc->pid, signum);

    if (signum == SIGSTOP) {
        // Suspend the process
        // TODO: suspend other threads in multithreaded process
        _assert(proc);
        proc->proc_state = PROC_SUSPENDED;

        // Notify parent of suspension
        if (proc->pid_notify.owner) {
            thread_notify_io(&proc->pid_notify);
        }

        while (proc->proc_state == PROC_SUSPENDED) {
            sched_unqueue(thr, THREAD_WAITING);
        }
        _assert(proc->proc_state == PROC_ACTIVE);

        return;
    }

    assert(thr->signal_entry != MM_NADDR, "Thread hasn't set its signal entry!\n");
    assert(thr->signal_stack_base != MM_NADDR, "Thread hasn't set its signal stack!\n");

    uintptr_t old_rsp0_top = thr->data.rsp0_top;
    uintptr_t signal_rsp3 = thr->signal_stack_base + thr->signal_stack_size;

    context_sigenter(thr->signal_entry, signal_rsp3, signum);

    thr->data.rsp0_top = old_rsp0_top;
}

__attribute__((noreturn)) void sys_exit(int status) {
    struct thread *thr = thread_self;
    struct process *proc = thr->proc;

    if (proc->thread_count != 1) {
        // Stop the thread
        kdebug("Thread <%p> exiting\n", thr);

        // Clear pending I/O (if exiting from signal interrupting select())
        if (!list_empty(&thr->wait_head)) {
            thread_wait_io_clear(thr);
        }

        --proc->thread_count;

        sched_unqueue(thr, THREAD_STOPPED);
        panic("This code shouldn't run\n");
    }
    kdebug("Process %d exited with status %d\n", proc->pid, status);

    // Clear pending I/O (if exiting from signal interrupting select())
    if (!list_empty(&thr->wait_head)) {
        thread_wait_io_clear(thr);
    }

    proc->exit_status = status;

    // Notify waitpid()ers
    if (proc->pid_notify.owner) {
        thread_notify_io(&proc->pid_notify);
    }

    proc->proc_state = PROC_FINISHED;
    sched_unqueue(thr, THREAD_STOPPED);
    panic("This code shouldn't run\n");
}

void sys_sigreturn(void) {
    context_sigreturn();
}

static void process_signal_thread(struct thread *thr, int signum) {
    struct process *proc = thr->proc;

    if (thr->sleep_notify.owner) {
        thread_notify_io(&thr->sleep_notify);
    }

    if (thr->cpu == (int) get_cpu()->processor_id) {
        if (thr == thread_self) {
            kdebug("Signal will be handled now\n");
            thread_sigenter(signum);
        } else {
            kdebug("Signal will be handled later\n");
            process_signal_set(proc, signum);

            sched_queue(thr);
        }
    } else if (thr->cpu >= 0) {
        kdebug("Signal will be handled later (other cpu%d)\n", thr->cpu);
        process_signal_set(proc, signum);

        sched_queue(thr);
    } else {
        kdebug("Signal will be handled later (not running)\n");
        process_signal_set(proc, signum);

        sched_queue(thr);
    }
}

void process_signal(struct process *proc, int signum) {
    if (proc->proc_state == PROC_SUSPENDED) {
        // Can only KILL or CONT suspended process
        if (signum == SIGCONT) {
            proc->proc_state = PROC_ACTIVE;
        }
        if (signum != SIGKILL && signum != SIGCONT) {
            return;
        }
    }

    struct thread *thr;
    list_for_each_entry(thr, &proc->thread_list, thread_link) {
        if (thr->state != THREAD_STOPPED) {
            return process_signal_thread(thr, signum);
        }
    }

    panic("Failed to deliver the signal\n");
}

int thread_check_signal(struct thread *thr, int ret) {
    struct process *proc = thr->proc;
    if (!proc) {
        return ret;
    }

    if (proc->sigq) {
        // Pick one signal to handle at a time
        int signum = 0;
        for (int i = 0; i < 64; ++i) {
            if (proc->sigq & (1ULL << i)) {
                proc->sigq &= ~(1ULL << i);
                signum = i + 1;
                break;
            }
        }
        _assert(signum);
        thread_sigenter(signum);

        // Theoretically, a rogue thread could steal all the CPU time by sending itself signals
        // in normal context, as after returning from thread_sigenter() this code will return
        // to a normal execution
        // XXX: Maybe makes sense to just yield() here
        return -EINTR;
    }

    return ret;
}

int sys_kill(pid_t pid, int signum) {
    struct process *proc;

    if (pid > 0) {
        proc = process_find(pid);
    } else if (pid == 0) {
        proc = thread_self->proc;
    } else if (pid < -1) {
        // TODO: check if there's any process in that group
        process_signal_pgid(-pid, signum);

        return 0;
    } else {
        panic("Not implemented\n");
    }

    if (!proc || proc->proc_state == PROC_FINISHED) {
        return -ESRCH;
    }

    if (signum == 0) {
        return 0;
    }

    if (signum <= 0 || signum >= 64) {
        return -EINVAL;
    }

    if (!proc) {
        // No such process
        return -ESRCH;
    }

    process_signal(proc, signum);

    return 0;
}

void sys_sigentry(uintptr_t entry) {
    thread_self->signal_entry = entry;
}

int sys_sigaltstack(const struct user_stack *ss, struct user_stack *old_ss) {
    struct thread *thr = thread_self;
    _assert(thr);

    if (old_ss) {
        old_ss->ss_sp = (void *) thr->signal_stack_base;
        old_ss->ss_size = thr->signal_stack_size;
    }
    if (ss) {
        thr->signal_stack_base = (uintptr_t) ss->ss_sp;
        thr->signal_stack_size = ss->ss_size;
    }

    return 0;
}

pid_t sys_getpid(void) {
    _assert(thread_self && thread_self->proc);
    return thread_self->proc->pid;
}

pid_t sys_getpgid(pid_t pid) {
    struct process *proc;

    if (pid == 0) {
        proc = thread_self->proc;
    } else {
        proc = process_find(pid);
    }

    if (!proc) {
        return -ESRCH;
    }

    return proc->pgid;
}

int sys_setpgid(pid_t pid, pid_t pgrp) {
    struct process *proc = thread_self->proc;

    if (pid == 0 && pgrp == 0) {
        proc->pgid = proc->pid;
        return 0;
    }

    if (pid == proc->pid) {
        proc->pgid = pgrp;
        return 0;
    }
    // Find child with pid pid (guess only children can be setpgid'd)
    struct process *chld = process_child(proc, pid);
    if (!chld) {
        return -ESRCH;
    }
    if (chld->pgid != proc->pgid) {
        return -EACCES;
    }
    chld->pgid = pgrp;

    return 0;
}

int sys_setuid(uid_t uid) {
    struct process *proc = thread_self->proc;
    _assert(proc);

    if (proc->ioctx.uid != 0) {
        return -EACCES;
    }

    proc->ioctx.uid = uid;
    return 0;
}

int sys_setgid(gid_t gid) {
    struct process *proc = thread_self->proc;
    _assert(proc);

    if (proc->ioctx.gid != 0 && proc->ioctx.uid != 0) {
        return -EACCES;
    }

    proc->ioctx.gid = gid;
    return 0;
}

uid_t sys_getuid(void) {
    return thread_self->proc->ioctx.uid;
}

gid_t sys_getgid(void) {
    return thread_self->proc->ioctx.gid;
}
