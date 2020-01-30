#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/pool.h"
#include "sys/amd64/cpu.h"
#include "sys/vmalloc.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "sys/mm.h"

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

////

static pid_t last_kernel_pid = 0;
static pid_t last_user_pid = 0;

pid_t thread_alloc_pid(int is_user) {
    if (is_user) {
        return ++last_user_pid;
    } else {
        return -(++last_kernel_pid);
    }
}

////

int thread_init(struct thread *thr, uintptr_t entry, void *arg, int user) {
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

    return 0;
}

int sys_fork(struct sys_fork_frame *frame) {
    static int nfork = 0;
    static struct thread forkt[3] = {0};

    struct thread *dst = &forkt[nfork++];
    struct thread *src = thread_self;

    uintptr_t stack_pages = amd64_phys_alloc_page();
    _assert(stack_pages != MM_NADDR);

    dst->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    dst->data.rsp0_size = MM_PAGE_SIZE;
    dst->data.rsp0_top = dst->data.rsp0_base + dst->data.rsp0_size;

    mm_space_t space = amd64_mm_pool_alloc();
    mm_space_fork(space, (mm_space_t) MM_VIRTUALIZE(src->data.cr3), MM_CLONE_FLG_KERNEL | MM_CLONE_FLG_USER);

    dst->data.rsp3_base = src->data.rsp3_base;
    dst->data.rsp3_size = src->data.rsp3_size;

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

    dst->data.rsp0 = (uintptr_t) stack;

    // Allocate a new PID for userspace thread
    dst->pid = thread_alloc_pid(1);

    sched_queue(dst);

    return dst->pid;
}

__attribute__((noreturn)) void sys_exit(int status) {
    struct thread *thr = thread_self;
    kdebug("Thread %d exited with status %d\n", thr->pid, status);

    sched_unqueue(thr);
    panic("This code shouldn't run\n");
}
