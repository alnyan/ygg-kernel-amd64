#include "sys/amd64/mm/mm.h"
#include "sys/amd64/mm/pool.h"
#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/string.h"
#include "sys/heap.h"
#include "sys/vmalloc.h"
#include "sys/errno.h"
#include "sys/fs/vfs.h"
#include "sys/fcntl.h"
#include "sys/sched.h"
#include "sys/tty.h"
#include "sys/debug.h"
#include "sys/mm.h"

#define THREAD_KSTACK_SIZE      32768

int thread_init(
        struct thread *t,
        mm_space_t space,
        uintptr_t entry,
        uintptr_t rsp0_base,
        size_t rsp0_size,
        uintptr_t rsp3_base,
        size_t rsp3_size,
        uint32_t flags,
        void *arg) {
    if (!rsp0_base) {
        void *kstack = kmalloc(THREAD_KSTACK_SIZE);
        _assert(kstack);

        rsp0_base = (uintptr_t) kstack;
        rsp0_size = THREAD_KSTACK_SIZE;
    }

    if (!(flags & THREAD_KERNEL) && !rsp3_base) {
        uintptr_t ustack = vmalloc(space, 0x100000, 0xF0000000, 4, VM_ALLOC_USER | VM_ALLOC_WRITE);
        _assert(ustack != MM_NADDR);

        rsp3_base = ustack;
        rsp3_size = 4 * 0x1000;

        kdebug("Allocated user stack: %p\n", ustack);
    }

    t->data.stack0_base = rsp0_base;
    t->data.stack0_size = rsp0_size;
    t->data.rsp0 = t->data.stack0_base + t->data.stack0_size - sizeof(struct cpu_context);

    if (!(flags & THREAD_KERNEL)) {
        t->data.stack3_base = rsp3_base;
        t->data.stack3_size = rsp3_size;
    }

    // Setup context
    struct cpu_context *ctx = (struct cpu_context *) t->data.rsp0;

    ctx->rip = entry;
    ctx->rflags = 0x248;

    if (flags & THREAD_KERNEL) {
        ctx->rsp = t->data.stack0_base + t->data.stack0_size;
        ctx->cs = 0x08;
        ctx->ss = 0x10;

        ctx->ds = 0x10;
        ctx->es = 0x10;
        ctx->fs = 0;
    } else {
        ctx->rsp = t->data.stack3_base + t->data.stack3_size;
        ctx->cs = 0x23;
        ctx->ss = 0x1B;

        ctx->ds = 0x1B;
        ctx->es = 0x1B;
        ctx->fs = 0;
    }

    ctx->rax = 0;
    ctx->rcx = 0;
    ctx->rdx = 0;
    ctx->rdx = 0;
    ctx->rbp = 0;
    ctx->rsi = 0;
    ctx->rdi = (uintptr_t) arg;

    ctx->cr3 = ({ _assert((uintptr_t) space >= 0xFFFFFF0000000000); (uintptr_t) space - 0xFFFFFF0000000000; });

    ctx->r8 = 0;
    ctx->r9 = 0;
    ctx->r10 = 0;
    ctx->r11 = 0;
    ctx->r12 = 0;
    ctx->r13 = 0;
    ctx->r14 = 0;
    ctx->r15 = 0;

    ctx->__canary = AMD64_STACK_CTX_CANARY;

    t->space = space;
    t->flags = flags;
    t->next = NULL;

    memset(&t->ioctx, 0, sizeof(t->ioctx));
    memset(t->fds, 0, sizeof(t->fds));

    return 0;
}

int sys_fork(void) {
    asm volatile ("cli");
    int res;
    struct thread *thr_src = get_cpu()->thread;
    _assert(thr_src);

    struct thread *thr_dst = (struct thread *) kmalloc(sizeof(struct thread));
    if (!thr_dst) {
        return -ENOMEM;
    }

    // Clone memory space
    mm_space_t thread_space = amd64_mm_pool_alloc();
    if (!thread_space) {
        kfree(thr_dst);
        return -ENOMEM;
    }

    if ((res = mm_space_fork(thread_space, thr_src->space, MM_CLONE_FLG_USER | MM_CLONE_FLG_KERNEL)) < 0) {
        kfree(thr_dst);
        amd64_mm_pool_free(thread_space);
        return res;
    }

    // Clone process state
    void *dst_kstack = kmalloc(THREAD_KSTACK_SIZE);
    struct amd64_thread *dst_data = &thr_dst->data;
    _assert(dst_kstack);

    dst_data->stack0_base = (uintptr_t) dst_kstack;
    dst_data->stack0_size = THREAD_KSTACK_SIZE;
    dst_data->stack3_base = thr_src->data.stack3_base;
    dst_data->stack3_size = thr_src->data.stack3_size;
    dst_data->rsp0 = dst_data->stack0_base + dst_data->stack0_size - sizeof(struct cpu_context);

    // Clone cpu context
    struct cpu_context *src_ctx = (struct cpu_context *) thr_src->data.rsp0;
    struct cpu_context *dst_ctx = (struct cpu_context *) thr_dst->data.rsp0;

    memcpy(dst_ctx, src_ctx, sizeof(struct cpu_context));
    dst_ctx->cr3 = MM_PHYS(thread_space);
    // Return with zero from fork()
    dst_ctx->rax = 0;

    // Finish up
    thr_dst->space = thread_space;
    thr_dst->flags = thr_src->flags;
    thr_dst->next = NULL;

    // TODO: clone FDs
    memset(&thr_dst->ioctx, 0, sizeof(thr_dst->ioctx));
    memset(thr_dst->fds, 0, sizeof(thr_dst->fds));

    thr_dst->fds[0].flags = O_RDONLY;
    thr_dst->fds[0].vnode = tty0;
    thr_dst->fds[0].pos = 0;

    thr_dst->fds[1].flags = O_WRONLY;
    thr_dst->fds[1].vnode = tty0;
    thr_dst->fds[1].pos = 0;

    sched_add(thr_dst);

    // Parent fork() returns with PID
    src_ctx->rax = thr_dst->pid;

    return 0;
}

void amd64_thread_set_ip(struct thread *t, uintptr_t ip) {
    struct cpu_context *ctx = (struct cpu_context *) t->data.rsp0;

    ctx->rip = ip;
    ctx->rflags = 0x248;
}
