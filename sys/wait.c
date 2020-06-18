#include "arch/amd64/hw/timer.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/debug.h"
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
        int r = thread_check_signal(t, 0);
        if (r != 0) {
            n->owner = NULL;
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
            n->value = 0;
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
    thr->sleep_deadline = deadline;
    timer_add_sleep(thr);
    return thread_wait_io(thr, &thr->sleep_notify);
    //// Store time when interrupt occured
    //if (int_time) {
    //    *int_time = system_time;
    //}
}

