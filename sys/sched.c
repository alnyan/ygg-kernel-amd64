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
struct thread *thread_current = NULL;

static void *idle(void *arg) {
    while (1) {
        asm volatile ("hlt");
    }
    return 0;
}

////

void sched_queue(struct thread *thr) {
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

void sched_unqueue(struct thread *thr) {
    struct thread *prev = thr->prev;
    struct thread *next = thr->next;

    if (next == thr) {
        queue_head = NULL;
        thread_current = &thread_idle;
        context_switch_to(&thread_idle, thr);
        return;
    }

    if (thr == queue_head) {
        queue_head = next;
    }

    next->prev = prev;
    prev->next = next;

    thr->prev = NULL;
    thr->next = NULL;

    if (thr == thread_current) {
        thread_current = next;
        context_switch_to(next, thr);
    }
}

void yield(void) {
    struct thread *from = thread_current;
    struct thread *to;

    if (from && from->next) {
        to = from->next;
    } else if (queue_head) {
        to = queue_head;
    } else {
        to = &thread_idle;
    }

    thread_current = to;
    context_switch_to(to, from);
}

void sched_init(void) {
    thread_init(&thread_idle, (uintptr_t) idle, 0, 0);
    thread_idle.pid = -1;
}

void sched_enter(void) {
    extern void amd64_irq0(void);
    amd64_idt_set(0, 32, (uintptr_t) amd64_irq0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    if (queue_head) {
        thread_current = queue_head;
        context_switch_first(thread_current);
    } else {
        thread_current = &thread_idle;
        context_switch_first(&thread_idle);
    }
}
