#include "arch/amd64/context.h"
#include "sys/mem/vmalloc.h"
#include "sys/mem/phys.h"
#include "user/errno.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"

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

struct process *task_start(void *entry, void *arg, int flags) {
    struct process *proc = kmalloc(sizeof(struct process));
    if (!proc) {
        return NULL;
    }

    if (process_init_thread(proc, (uintptr_t) entry, arg, 0) != 0) {
        kfree(proc);
        return NULL;
    }

    sched_queue(process_first_thread(proc));

    return proc;
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
    uintptr_t stack_pages = mm_phys_alloc_contiguous(THREAD_KSTACK_PAGES, PU_KERNEL);
    _assert(stack_pages != MM_NADDR);

    thr->signal_entry = MM_NADDR;
    thr->signal_stack_base = MM_NADDR;
    thr->signal_stack_size = 0;
    thr->sched_prev = NULL;
    thr->sched_next = NULL;

    thr->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    thr->data.rsp0_size = MM_PAGE_SIZE * THREAD_KSTACK_PAGES;
    thr->data.rsp0_top = thr->data.rsp0_base + thr->data.rsp0_size;
    thr->flags = (flags & THR_INIT_USER) ? 0 : THREAD_KERNEL;
    thr->sigq = 0;
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
            ustack_base = vmfind(space, THREAD_USTACK_BEGIN, THREAD_USTACK_END, THREAD_USTACK_PAGES);
            _assert(ustack_base != MM_NADDR);
            for (size_t i = 0; i < THREAD_USTACK_PAGES; ++i) {
                uintptr_t phys = mm_phys_alloc_page(PU_PRIVATE);
                _assert(phys != MM_NADDR);
                mm_map_single(space, ustack_base + i * MM_PAGE_SIZE, phys, MM_PAGE_WRITE | MM_PAGE_USER);
            }
            thr->data.rsp3_base = ustack_base;
            thr->data.rsp3_size = MM_PAGE_SIZE * THREAD_USTACK_PAGES;
        }
    }

    thr->state = THREAD_READY;

    // Initial thread context
    // Entry context
    if (flags & THR_INIT_USER) {
        // ss
        *--stack = 0x1B;
        // rsp
        // Subtract a single word, as x86_64 calling conventions expect
        // the "caller" to place a return address there
        *--stack = thr->data.rsp3_base + thr->data.rsp3_size - 8;
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

void thread_dump(int level, struct thread *thr) {
    if (thr) {
        debugf(level, "In thread <%p>\n", thr);
        if (thr->proc) {
            struct process *proc = thr->proc;
            debugf(level, "of process #%d (%s)\n", proc->pid, proc->name);

            if (proc->thread_count > 1) {
                debugf(level, "Thread list:\n");
                struct thread *thr;
                list_for_each_entry(thr, &proc->thread_list, thread_link) {
                    debugf(level, " - <%p>\n", thr);
                }
            }
        } else {
            debugf(level, "of no process\n");
        }
    }
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
    kdebug("Process #%d (%s) exited with status %d\n", proc->pid, proc->name, status);

    // Clear pending I/O (if exiting from signal interrupting select())
    if (!list_empty(&thr->wait_head)) {
        thread_wait_io_clear(thr);
    }

    // Close FDs even before being reaped
    for (size_t i = 0; i < THREAD_MAX_FDS; ++i) {
        if (proc->fds[i]) {
            ofile_close(&proc->ioctx, proc->fds[i]);
            proc->fds[i] = NULL;
        }
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

void sys_yield(void) {
    yield();
}

void sys_sigreturn(void) {
    context_sigreturn();
}

void thread_signal(struct thread *thr, int signum) {
    struct process *proc = thr->proc;

    if (thr->sleep_notify.owner) {
        thread_notify_io(&thr->sleep_notify);
    }

    if (thr->cpu == (int) get_cpu()->processor_id && thr == thread_self) {
        kdebug("Signal will be handled now\n");
        thread_sigenter(signum);
    } else {
        kdebug("Signal will be handled later\n");
        if (signal_is_exception(signum)) {
            // Exceptions will only be handled by this thread
            xxx_signal_set(thr, signum);
        } else {
            // Delegate handling to anybody else
            xxx_signal_set(proc, signum);
        }

        // TODO: for process signals, only wakeup a thread if there's no one else to handle it
        sched_queue(thr);
    }
}

int thread_check_signal(struct thread *thr, int ret) {
    struct process *proc = thr->proc;
    if (!proc) {
        return ret;
    }

    // Check exceptions first
    if (thr->sigq) {
        panic("!!!\n");
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

