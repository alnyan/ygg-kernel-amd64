#include "arch/amd64/hw/timer.h"
#include "arch/amd64/cpu.h"
#include "user/errno.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "user/wait.h"
#include "sys/wait.h"

void thread_wait_io_init(struct io_notify *n) {
    n->owner = NULL;
    n->value = 0;
    n->lock = 0;
    list_head_init(&n->link);
    // For wait_io_any
    list_head_init(&n->own_link);
}

int thread_wait_io(struct thread *t, struct io_notify *n) {
    uintptr_t irq;
    while (1) {
        spin_lock_irqsave(&n->lock, &irq);
        // Check value
        if (n->value) {
            // Consume value
            n->value = 0;
            spin_release_irqrestore(&n->lock, &irq);
            return 0;
        }

        // Wait for the value to change
        // TODO: multiple threads waiting on same io_notify
        _assert(!n->owner);
        n->owner = t;
        spin_release_irqrestore(&n->lock, &irq);

        sched_unqueue(t, THREAD_WAITING);

        // Check if we were interrupted during io wait
        n->owner = NULL;
        int r = thread_check_signal(t, 0);
        if (r != 0) {
            return r;
        }
    }
}

void thread_notify_io(struct io_notify *n) {
    uintptr_t irq;
    struct thread *t = NULL;
    spin_lock_irqsave(&n->lock, &irq);
    ++n->value;
    t = n->owner;
    n->owner = NULL;
    spin_release_irqrestore(&n->lock, &irq);

    if (t) {
        // Prevent double task wakeup when
        // two event sources simultaneously try
        // to do it
        if (t->sched_next) {
            return;
        }
        sched_queue(t);
    }
}

void thread_wait_io_add(struct thread *thr, struct io_notify *n) {
    uintptr_t irq;
    _assert(n);
    spin_lock_irqsave(&n->lock, &irq);
    _assert(!n->owner);
    n->owner = thr;
    list_add(&n->own_link, &thr->wait_head);
    spin_release_irqrestore(&n->lock, &irq);
}

int thread_wait_io_any(struct thread *thr, struct io_notify **r_n) {
    uintptr_t irq;
    struct io_notify *n, *it;

    while (1) {
        // Check if any of values are non-zero
        n = NULL;
        list_for_each_entry(it, &thr->wait_head, own_link) {
            spin_lock_irqsave(&it->lock, &irq);
            if (it->value) {
                n = it;
                break;
            }
            spin_release_irqrestore(&it->lock, &irq);
        }

        if (n) {
            // Found ready descriptor
            --n->value;
            spin_release_irqrestore(&it->lock, &irq);
            *r_n = n;

            return 0;
        } else {
            // Wait
            // TODO: reset owners
            sched_unqueue(thr, THREAD_WAITING);

            int r = thread_check_signal(thr, 0);
            if (r != 0) {
                return r;
            }
        }
    }
}

void thread_wait_io_clear(struct thread *t) {
    while (!list_empty(&t->wait_head)) {
        struct list_head *h = t->wait_head.next;
        struct io_notify *n = list_entry(h, struct io_notify, own_link);
        // TODO: maybe check here for sleep descriptors and cancel sleeps if needed
        n->owner = NULL;
        list_del_init(h);
    }
}

int thread_sleep(struct thread *thr, uint64_t deadline, uint64_t *int_time) {
    // Cancel previous sleep
    list_del_init(&thr->sleep_notify.link);
    thr->sleep_notify.value = 0;

    thr->sleep_deadline = deadline;
    timer_add_sleep(thr);
    return thread_wait_io(thr, &thr->sleep_notify);
}

static int wait_check_pid(struct process *chld, int flags) {
    if (chld->proc_state == PROC_FINISHED) {
        return 0;
    } else if ((flags & WSTOPPED) && chld->proc_state == PROC_SUSPENDED) {
        return 0;
    }

    return -1;
}

static int wait_check_pgrp(struct process *proc_self, pid_t pgrp, int flags, struct process **chld) {
    for (struct process *_chld = proc_self->first_child; _chld; _chld = _chld->next_child) {
        if (pgrp == -1 || _chld->pgid == -pgrp) {
            if (wait_check_pid(_chld, flags) == 0) {
                *chld = _chld;
                return 0;
            }
        }
    }
    return -1;
}

int sys_waitpid(pid_t pid, int *status, int flags) {
    pid_t result_pid = -ECHILD;
    struct thread *thr = thread_self;
    _assert(thr);
    struct process *proc_self = thr->proc;
    _assert(proc_self);

    struct process *chld;
    struct io_notify *notify;
    int res;

    if (pid > 0) {
        chld = process_child(proc_self, pid);

        if (!chld) {
            return -ECHILD;
        }
    } else if (pid < -1) {
        int any_proc = 0;
        for (struct process *_chld = proc_self->first_child; _chld; _chld = _chld->next_child) {
            if (_chld->pgid == -pid) {
                ++any_proc;
            }
        }
        if (!any_proc) {
            return -ECHILD;
        }
    } else if (pid != -1) {
        panic("Not implemented: waitpid(%d, ...)\n", pid);
    }

    while (1) {
        if (pid > 0) {
            if (wait_check_pid(chld, flags) == 0) {
                break;
            }

            res = thread_wait_io(thr, &chld->pid_notify);
        } else if (pid <= -1) {
            // Check if anybody in pgrp has changed status
            if (wait_check_pgrp(proc_self, pid, flags, &chld) == 0) {
                _assert(chld);
                break;
            }

            // Build wait list
            for (struct process *_chld = proc_self->first_child; _chld; _chld = _chld->next_child) {
                if (pid == -1 || _chld->pgid == -pid) {
                    thread_wait_io_add(thr, &_chld->pid_notify);
                }
            }

            // Wait for any of pgrp
            res = thread_wait_io_any(thr, &notify);

            thread_wait_io_clear(thr);
        } else {
            panic("Shouldn't run\n");
        }

        if (res < 0) {
            // Likely interrupted
            return res;
        }
    }

    result_pid = chld->pid;
    if (chld->proc_state == PROC_FINISHED) {
        if (status) {
            *status = chld->exit_status;
        }

        // TODO: automatically cleanup threads which don't have
        //       a parent like PID 1
        process_unchild(chld);
        list_del(&chld->g_link);
        process_free(chld);
    } else if (chld->proc_state == PROC_SUSPENDED) {
        if (status) {
            // WIFSTOPPED
            *status = 127;
        }
    }

    return result_pid;
}
