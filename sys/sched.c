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

////

static void *idle(void *arg) {
    while (1) {
        asm volatile ("hlt");
    }
    return 0;
}

static void *t0(void *arg) {
    uint16_t *ptr = (uint16_t *) MM_VIRTUALIZE(0xB8000 + (uint64_t) arg * 2);
    size_t cntr = 0;

    *ptr = 0;
    while (1) {
        for (size_t i = 0; i < 10000000; ++i);

        *ptr ^= 'A' | 0x700;
        ++cntr;

        if (cntr == 20 * (((uint64_t) arg) +1)) {
            kdebug("[%d] Død\n", arg);
            sched_unqueue(thread_current);
        }
    }
    return 0;
}

static void *u0(void *arg) {
    int r;
    asm volatile ("syscall":"=a"(r):"a"(123));

    if (r == 0) {
        arg = (void *) ((uintptr_t) arg + 2);
    }

    uint16_t *ptr = (uint16_t *) MM_VIRTUALIZE(0xB8030 + arg);
    size_t cntr = 0;

    *ptr = 0;
    while (1) {
        *ptr ^= '0' | 0xD00;
        for (size_t i = 0; i < 1000000; ++i);

        if (cntr == 1000 && r == 0) {
            *ptr = 'X' | 0x300;
            asm volatile ("xorq %rdi, %rdi; movq $60, %rax; syscall");
        }
        ++cntr;
    }

    return 0;
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

static struct thread t_n[3] = {0};
static struct thread t_u[3] = {0};

void sched_init(void) {
    thread_init(&thread_idle, (uintptr_t) idle, 0, 0);
    thread_idle.pid = -1;

    for (size_t i = 0; i < 3; ++i) {
        thread_init(&t_n[i], (uintptr_t) t0, (void *) i, 0);
        t_n[i].pid = thread_alloc_pid(0);
    }

    thread_init(&t_u[0], (uintptr_t) u0, (void *) 0, 1);
    t_u[0].pid = thread_alloc_pid(1);

    sched_queue(&t_n[0]);
    sched_queue(&t_n[1]);
    sched_queue(&t_n[2]);

    sched_queue(&t_u[0]);
}

void sched_enter(void) {
    extern void amd64_irq0(void);
    amd64_idt_set(0, 32, (uintptr_t) amd64_irq0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    thread_current = queue_head;
    context_switch_first(thread_current);
}
