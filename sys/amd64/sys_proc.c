#include "sys/amd64/sys_proc.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/signum.h"
#include "sys/sched.h"
#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/panic.h"

void sys_exit(int status) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    kdebug("%u exited with code %d\n", thr->pid, status);
    thr->exit_code = status;
    //thr->flags |= THREAD_STOPPED;
    //amd64_syscall_yield_stopped();
}

int sys_kill(int pid, int signum) {
    asm volatile ("cli");
    struct thread *cur_thread = get_cpu()->thread;

    // Find destination thread
    struct thread *dst_thread = NULL;// = sched_find(pid);

    if (!dst_thread) {
        return -ESRCH;
    }

    // Push an item to signal queue
    thread_signal(dst_thread, signum);

    if (cur_thread == dst_thread) {
        // Suicide signal, just hang on and wait
        // until scheduler decides it's our time
        while (cur_thread->sigq) {
            asm volatile ("sti; hlt; cli");
        }
    }

    return 0;
}

void sys_signal(uintptr_t entry) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    kdebug("%u set its signal handler to %p\n", thr->pid, entry);
    thr->sigentry = entry;
}

void sys_sigret(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    //thr->flags |= THREAD_SIGRET;

    asm volatile ("sti; hlt; cli");
}

int sys_waitpid(int pid, int *status) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    // Find child with such PID
    struct thread *waited = NULL;
    int wait_good = 0;

    for (struct thread *child = thr->child; child; child = child->next_child) {
        if (!(child->flags & THREAD_KERNEL) && (int) child->pid == pid) {
            waited = child;
            break;
        }
    }

    if (!waited) {
        return -ESRCH;
    }

    while (1) {
        //if (waited->flags & THREAD_STOPPED) {
        //    kdebug("%d is done waiting for %d\n", thr->pid, waited->pid);
        //    wait_good = 1;
        //    break;
        //}
        asm volatile ("sti; hlt; cli");
    }

    if (wait_good) {
        if (status) {
            *status = waited->exit_code;
        }
        //waited->flags |= THREAD_DONE_WAITING;

        //thread_terminate(waited);
    }

    return 0;
}

int sys_getpid(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return thr->pid;
}

int sys_setuid(uid_t uid) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (thr->ioctx.uid != 0) {
        return -EACCES;
    }

    thr->ioctx.uid = uid;
    return 0;
}

int sys_setgid(gid_t gid) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (thr->ioctx.gid != 0 && thr->ioctx.uid != 0) {
        return -EACCES;
    }

    thr->ioctx.gid = gid;
    return 0;
}

uid_t sys_getuid(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return thr->ioctx.uid;
}

gid_t sys_getgid(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return thr->ioctx.gid;
}

pid_t sys_getpgid(pid_t pid) {
    struct thread *thr;

    if (pid == 0) {
        thr = get_cpu()->thread;
        _assert(thr);
    } else {
        thr = NULL;
        //thr = sched_find(pid);
    }

    if (!thr) {
        return -ESRCH;
    }

    return thr->pgid;
}

int sys_setpgid(pid_t pid, pid_t pgrp) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (pid == 0 && pgrp == 0) {
        thr->pgid = thr->pid;
        return 0;
    }

    // Find child with pid pid (guess only children can be setpgid'd)
    struct thread *ch;
    for (ch = thr->child; ch; ch = ch->next_child) {
        if (ch->pid == pid) {
            if (ch->pgid != thr->pgid) {
                return -EACCES;
            }

            ch->pgid = pgrp;
            return 0;
        }
    }

    return -ESRCH;
}

int sys_unknown(int no) {
    asm volatile ("cli");
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    thread_signal(thr, SIGSYS);

    // Suicide signal, just hang on and wait
    // until scheduler decides it's our time
    while (thr->sigq) {
        asm volatile ("sti; hlt; cli");
    }

    return -EINVAL;
}
