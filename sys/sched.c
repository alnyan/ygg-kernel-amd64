#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/pool.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/cpu.h"
#include "sys/vmalloc.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/mm.h"

// Enter a newly-created task
extern void context_enter(struct thread *thr);
// Stores current task context, loads new one's
extern void context_switch_to(struct thread *thr, struct thread *from);
// No current task, only load the first task to begin execution
extern void context_switch_first(struct thread *thr);

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

static void init_thread(struct thread *thr, void *(*entry)(void *), void *arg, int user) {
    uintptr_t stack_pages = amd64_phys_alloc_page();
    _assert(stack_pages != MM_NADDR);

    thr->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    thr->data.rsp0_size = MM_PAGE_SIZE;
    thr->data.rsp0_top = thr->data.rsp0_base + thr->data.rsp0_size;

    uint64_t *stack = (uint64_t *) (thr->data.rsp0_base + thr->data.rsp0_size);

    if (user) {
        mm_space_t space = amd64_mm_pool_alloc();
        mm_space_clone(space, mm_kernel, MM_CLONE_FLG_KERNEL);
        thr->data.cr3 = MM_PHYS(space);

        uintptr_t ustack_base = vmalloc(space, 0x1000000, 0xF0000000, 4, MM_PAGE_WRITE | MM_PAGE_USER);
        thr->data.rsp3_base = ustack_base;
        thr->data.rsp3_size = MM_PAGE_SIZE * 4;

        // Allow this thread to access upper pages for testing
        space[AMD64_MM_STRIPSX(KERNEL_VIRT_BASE) >> 39] |= MM_PAGE_USER;
        uint64_t *pdpt = (uint64_t *) MM_VIRTUALIZE(space[AMD64_MM_STRIPSX(KERNEL_VIRT_BASE) >> 39] & ~0xFFF);
        for (uint64_t i = 0; i < 4; ++i) {
            pdpt[((AMD64_MM_STRIPSX(KERNEL_VIRT_BASE) >> 30) + i) & 0x1FF] |= MM_PAGE_USER;
        }
    } else {
        thr->data.cr3 = MM_PHYS(mm_kernel);
    }

    // Initial thread context
    // Entry context
    if (user) {
        // ss
        *--stack = 0x1B;
        // rsp
        *--stack = thr->data.rsp3_base + thr->data.rsp3_size;
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
}

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

struct sys_fork_frame {
    uint64_t rdi, rsi, rdx, rcx;
    uint64_t r8, r9, r10, r11;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rflags;
    uint64_t rip;
};

int sys_fork(struct sys_fork_frame *frame) {
    static int nfork = 0;
    static struct thread forkt[3] = {0};

    struct thread *dst = &forkt[nfork++];
    struct thread *src = thread_current;

    uintptr_t stack_pages = amd64_phys_alloc_page();
    _assert(stack_pages != MM_NADDR);

    dst->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    dst->data.rsp0_size = MM_PAGE_SIZE;
    dst->data.rsp0_top = dst->data.rsp0_base + dst->data.rsp0_size;

    mm_space_t space = amd64_mm_pool_alloc();
    mm_space_fork(space, (mm_space_t) MM_VIRTUALIZE(src->data.cr3), MM_CLONE_FLG_KERNEL | MM_CLONE_FLG_USER);

    space[AMD64_MM_STRIPSX(KERNEL_VIRT_BASE) >> 39] |= MM_PAGE_USER;
    uint64_t *pdpt = (uint64_t *) MM_VIRTUALIZE(space[AMD64_MM_STRIPSX(KERNEL_VIRT_BASE) >> 39] & ~0xFFF);
    for (uint64_t i = 0; i < 4; ++i) {
        pdpt[((AMD64_MM_STRIPSX(KERNEL_VIRT_BASE) >> 30) + i) & 0x1FF] |= MM_PAGE_USER;
    }

    dst->data.cr3 = MM_PHYS(space);

    uint64_t *stack = (uint64_t *) (dst->data.rsp0_base + dst->data.rsp0_size);

    // Initial thread context
    // Entry context
    // ss
    *--stack = 0x1B;
    // rsp
    *--stack = frame->rsp;
    // rflags
    _assert(frame->rflags & 0x200);
    *--stack = frame->rflags;
    // cs
    *--stack = 0x23;
    // rip
    *--stack = frame->rip;

    // Caller-saved
    // r11
    *--stack = frame->r11;
    // r10
    *--stack = frame->r10;
    // r9
    *--stack = frame->r9;
    // r8
    *--stack = frame->r8;
    // rcx
    *--stack = frame->rcx;
    // rdx
    *--stack = frame->rdx;
    // rsi
    *--stack = frame->rsi;
    // rdi
    *--stack = frame->rdi;
    // rax
    *--stack = 0;

    // Small stub so that context switch enters the thread properly
    *--stack = (uintptr_t) context_enter;
    // Callee-saved
    // r15
    *--stack = frame->r15;
    // r14
    *--stack = frame->r14;
    // r13
    *--stack = frame->r13;
    // r12
    *--stack = frame->r12;
    // rbp
    *--stack = frame->rbp;
    // rbx
    *--stack = frame->rbx;

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

    dst->data.rsp0 = (uintptr_t) stack;
    dst->pid = nfork + 20;

    sched_queue(dst);

    return dst->pid;
}

static void *u0(void *arg) {
    int r;
    asm volatile ("syscall":"=a"(r):"a"(123));

    if (r == 0) {
        arg = (void *) ((uint64_t) arg + 5);
        uint16_t *ptr = (uint16_t *) MM_VIRTUALIZE(0xB8000 + (uint64_t) arg * 2);
        *ptr = 0;
        while (1) {
            for (size_t i = 0; i < 10000000; ++i);

            *ptr ^= 'A' | 0x1200;
        }
    }
    uint16_t *ptr = (uint16_t *) MM_VIRTUALIZE(0xB8000 + (uint64_t) arg * 2);
    *ptr = 0;

    while (1) {
        for (size_t i = 0; i < 10000000; ++i);

        *ptr ^= 'A' | 0x1200;
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
    init_thread(&t_n[0], t0, (void *) 0, 0);
    init_thread(&t_n[1], t0, (void *) 1, 0);
    init_thread(&t_n[2], t0, (void *) 2, 0);
    init_thread(&t_u[0], u0, (void *) 5, 1);
    init_thread(&t_u[1], u0, (void *) 6, 1);
    init_thread(&t_u[2], u0, (void *) 7, 1);
    init_thread(&thread_idle, idle, 0, 0);

    thread_idle.pid = -1;

    t_n[0].pid = 1;
    t_n[1].pid = 2;
    t_n[2].pid = 3;

    t_u[0].pid = 10;
    t_u[1].pid = 11;
    t_u[2].pid = 12;

    sched_queue(&t_n[0]);
    sched_queue(&t_n[1]);
    sched_queue(&t_n[2]);

    sched_queue(&t_u[0]);
    sched_queue(&t_u[1]);
    sched_queue(&t_u[2]);
}

void sched_enter(void) {
    extern void amd64_irq0(void);
    amd64_idt_set(0, 32, (uintptr_t) amd64_irq0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    thread_current = queue_head;
    context_switch_first(thread_current);
}
