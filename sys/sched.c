#include "sys/thread.h"
#include "sys/debug.h"

thread_t *sched_current = NULL;
static thread_t *sched_first = NULL, *sched_last = NULL;
static pid_t sched_pid_counter = 0;

pid_t sched_add(thread_t *t) {
    pid_t pid;
    if (thread_get(t)->flags & THREAD_KERNEL) {
        pid = 0;
    } else {
        pid = ++sched_pid_counter;
    }

    thread_get(t)->pid = pid;

    if (sched_first) {
        sched_last->next = t;
    } else {
        sched_first = t;
    }
    sched_last = t;

    return pid;
}

int sched(void) {
    if (!sched_current) {
        sched_current = sched_first;
        if (!sched_current) {
            return -1;
        }
        return 0;
    }

    // Plain round-robin scheduler
    thread_t *from = sched_current;
    thread_t *to = thread_next(from);

    if (to == NULL) {
        to = sched_first;
    }
    if (to == from) {
        return -1;
    }

    return 0;
}
