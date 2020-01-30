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
    struct cpu *cpu = get_cpu();
    struct thread *prev = thr->prev;
    struct thread *next = thr->next;

    if (next == thr) {
        queue_head = NULL;
        get_cpu()->thread = &thread_idle;
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

    if (thr == cpu->thread) {
        cpu->thread = next;
        context_switch_to(next, thr);
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

    get_cpu()->thread = to;
    context_switch_to(to, from);
}

static struct thread user_init;

static void user_init_func(void *arg) {
    kdebug("Starting user init\n");

    extern int sys_execve(const char *path, const char **argp, const char **envp);
    sys_execve(arg, NULL, NULL);

    while (1) {
        asm volatile ("hlt");
    }
}

void sched_user_init(uintptr_t base) {
    thread_init(&user_init, (uintptr_t) user_init_func, (void *) base, 0);
    user_init.pid = thread_alloc_pid(0);
    sched_queue(&user_init);
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

    get_cpu()->thread = first_task;
    context_switch_first(first_task);
}
