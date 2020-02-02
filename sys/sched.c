#include "sys/amd64/context.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "sys/mm.h"

void yield(void);

//// Thread queueing

static struct thread *queue_head = NULL;
static struct thread thread_idle = {0};

static void *idle(void *arg) {
    while (1) {
        asm volatile ("hlt");
    }
    return 0;
}

////

void sched_queue(struct thread *thr) {
    asm volatile ("cli");
    _assert(thr);
    thr->state = THREAD_READY;

    if (queue_head) {
        struct thread *queue_tail = queue_head->prev;

        queue_tail->next = thr;
        thr->prev = queue_tail;
        queue_head->prev = thr;
        thr->next = queue_head;
    } else {
        thr->next = thr;
        thr->prev = thr;

        queue_head = thr;
    }
}

void sched_unqueue(struct thread *thr, enum thread_state new_state) {
    asm volatile ("cli");
    struct cpu *cpu = get_cpu();
    struct thread *prev = thr->prev;
    struct thread *next = thr->next;

    _assert((new_state == THREAD_WAITING) ||
            (new_state == THREAD_STOPPED) ||
            (new_state == THREAD_WAITING_IO) ||
            (new_state == THREAD_WAITING_PID));
    thr->state = new_state;

    thread_check_signal(thr, 0);

    if (next == thr) {
        thr->next = NULL;
        thr->prev = NULL;

        queue_head = NULL;
        cpu->thread = &thread_idle;
        context_switch_to(&thread_idle, thr);
        return;
    }

    if (thr == queue_head) {
        queue_head = next;
    }

    _assert(thr && next && prev);

    next->prev = prev;
    prev->next = next;

    thr->prev = NULL;
    thr->next = NULL;

    if (thr == cpu->thread) {
        cpu->thread = next;
        context_switch_to(next, thr);
    }
}

static void sched_debug_tree(int level, struct thread *thr, int depth) {
    for (int i = 0; i < depth; ++i) {
        debugs(level, "  ");
    }

    debugf(level, "%d ", thr->pid);

    switch (thr->state) {
    case THREAD_RUNNING:
        debugs(level, "RUN ");
        break;
    case THREAD_READY:
        debugs(level, "RDY ");
        break;
    case THREAD_WAITING:
        debugs(level, "IDLE");
        break;
    case THREAD_WAITING_IO:
        debugs(level, "I/O ");
        break;
    case THREAD_STOPPED:
        debugs(level, "STOP");
        break;
    default:
        debugs(level, "UNKN");
        break;
    }

    if (thr->first_child) {
        debugs(level, " {\n");

        for (struct thread *child = thr->first_child; child; child = child->next_child) {
            sched_debug_tree(level, child, depth + 1);
        }

        for (int i = 0; i < depth; ++i) {
            debugs(level, "  ");
        }
        debugs(level, "}\n");
    } else {
        debugc(level, '\n');
    }
}

void sched_debug_cycle(uint64_t delta_ms) {
    struct thread *thr = queue_head;
    extern struct thread user_init;

    debugs(DEBUG_DEFAULT, "Process tree:\n");
    sched_debug_tree(DEBUG_DEFAULT, &user_init, 0);

    if (thr) {
        debugs(DEBUG_DEFAULT, "cpu: ");
        struct thread *queue_tail = thr->prev;
        while (1) {
            debugf(DEBUG_DEFAULT, "%d%c", thr->pid,
                    ((thr->state == THREAD_RUNNING) ? 'R' : (thr->state == THREAD_READY ? '-' : '?')));

            if (thr == queue_tail) {
                break;
            } else {
                debugs(DEBUG_DEFAULT, ", ");
            }
            thr = thr->next;
        }
        debugc(DEBUG_DEFAULT, '\n');
    } else {
        kdebug("--- IDLE\n");
    }
}

void yield(void) {
    struct thread *from = get_cpu()->thread;
    struct thread *to;

    if (from && from->next) {
        to = from->next;
    } else if (queue_head) {
        to = queue_head;
    } else {
        to = &thread_idle;
    }

    // Check if instead of switching to a proper thread context we
    // have to use signal handling
    thread_check_signal(from, 0);

    if (from) {
        from->state = THREAD_READY;
    }

    to->state = THREAD_RUNNING;
    get_cpu()->thread = to;

    context_switch_to(to, from);
}

void sched_init(void) {
    thread_init(&thread_idle, (uintptr_t) idle, 0, 0);
    thread_idle.pid = -1;
}

void sched_enter(void) {
    extern void amd64_irq0(void);
    amd64_idt_set(0, 32, (uintptr_t) amd64_irq0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    struct thread *first_task = queue_head;
    if (!first_task) {
        first_task = &thread_idle;
    }

    first_task->state = THREAD_RUNNING;
    get_cpu()->thread = first_task;
    context_switch_first(first_task);
}
