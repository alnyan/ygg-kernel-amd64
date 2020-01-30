#include "sys/amd64/mm/phys.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/mm.h"

// Enter a newly-created task
extern void context_enter(struct thread *thr);
// Stores current task context, loads new one's
extern void context_switch_to(struct thread *thr, struct thread *from);
// No current task, only load the first task to begin execution
extern void context_switch_first(struct thread *thr);

void yield(void);

////

static struct thread sched_threads[3] = {0};
static int cntr = 0;

static void init_thread(struct thread *thr, void *(*entry)(void *)) {
    uintptr_t stack_pages = amd64_phys_alloc_page();
    _assert(stack_pages != MM_NADDR);

    thr->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    thr->data.rsp0_size = MM_PAGE_SIZE;

    uint64_t *stack = (uint64_t *) (thr->data.rsp0_base + thr->data.rsp0_size);

    // Initial thread context

    // Entry context
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
    *--stack = 0x12345678;
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
}

static void *t0(void *arg) {
    size_t count = 0;
    int v = 0;
    while (1) {
        if ((count % 100000) == 0) {
            if (v) {
                *(uint16_t *) MM_VIRTUALIZE(0xB8000) = 'A' | 0x700;
            } else {
                *(uint16_t *) MM_VIRTUALIZE(0xB8000) = 0;
            }
            v = !v;
        }

        ++count;
    }
    return 0;
}

static void *t1(void *arg) {
    size_t count = 0;
    int v = 0;
    while (1) {
        if ((count % 100000) == 0) {
            if (v) {
                *(uint16_t *) MM_VIRTUALIZE(0xB8002) = 'B' | 0x700;
            } else {
                *(uint16_t *) MM_VIRTUALIZE(0xB8002) = 0;
            }
            v = !v;
        }

        ++count;
    }
    return 0;
}

static void *t2(void *arg) {
    size_t count = 0;
    int v = 0;
    while (1) {
        if ((count % 100000) == 0) {
            if (v) {
                *(uint16_t *) MM_VIRTUALIZE(0xB8004) = 'C' | 0x700;
            } else {
                *(uint16_t *) MM_VIRTUALIZE(0xB8004) = 0;
            }
            v = !v;
        }

        ++count;
    }
    return 0;
}

void yield(void) {
    struct thread *from = &sched_threads[cntr];

    cntr = (cntr + 1) % 3;

    struct thread *next = &sched_threads[cntr];
    context_switch_to(next, from);
}

void sched_init(void) {
    init_thread(&sched_threads[0], t0);
    init_thread(&sched_threads[1], t1);
    init_thread(&sched_threads[2], t2);
}

void sched_enter(void) {
    context_switch_first(&sched_threads[0]);
}
