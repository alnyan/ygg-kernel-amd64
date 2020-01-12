#include "sys/amd64/mm/mm.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/pool.h"
#include "sys/amd64/mm/map.h"
#include "sys/amd64/syscall.h"
#include "sys/binfmt_elf.h"
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

// Means stack0 wasn't kmalloc'd, so don't free it
#define THREAD_NOHEAP_STACK0    (1 << 0)
// Means stack3 wasn't kmalloc'd, so don't free it
#define THREAD_NOHEAP_STACK3    (1 << 1)

static uint64_t fxsave_buf[FXSAVE_REGION / 8] __attribute__((aligned(16)));

void thread_save_context(struct thread *thr) {
    _assert(thr);
    asm volatile ("fxsave (%0)"::"r"(fxsave_buf));
    memcpyq((uint64_t *) thr->data.save_zone0, fxsave_buf, FXSAVE_REGION / 8);
    thr->flags |= THREAD_CTX_SAVED;
}

void thread_restore_context(struct thread *thr) {
    _assert(thr);
    if (thr->flags & THREAD_CTX_SAVED) {
        memcpyq(fxsave_buf, (uint64_t *) thr->data.save_zone0, FXSAVE_REGION / 8);
        asm volatile ("fxrstor (%0)"::"r"(fxsave_buf));
    }
}

// Initialize platform context to default values
// Assumes everything was allocated before
// Requirements for calling:
// flags, space
// stack0_base, stack0_size, [stack3_base, stack3_size]
int thread_platctx_init(struct thread *t, uintptr_t entry, void *arg) {
    _assert(t->data.stack0_base != MM_NADDR);

    struct cpu_context *ctx = (struct cpu_context *) t->data.rsp0;

    ctx->rip = entry;
    ctx->rflags = 0x248;

    if (t->flags & THREAD_KERNEL) {
        ctx->rsp = t->data.stack0_base + t->data.stack0_size;
        ctx->cs = 0x08;
        ctx->ss = 0x10;

        ctx->ds = 0x10;
        ctx->es = 0x10;
        ctx->fs = 0;
    } else {
        _assert(t->data.stack3_base != MM_NADDR);

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

    ctx->r8 = 0;
    ctx->r9 = 0;
    ctx->r10 = 0;
    ctx->r11 = 0;
    ctx->r12 = 0;
    ctx->r13 = 0;
    ctx->r14 = 0;
    ctx->r15 = 0;

    ctx->__canary = AMD64_STACK_CTX_CANARY;

    ctx->cr3 = ({
        _assert((uintptr_t) t->space >= 0xFFFFFF0000000000);
        (uintptr_t) t->space - 0xFFFFFF0000000000;
    });

    return 0;
}

static int thread_ioctx_init(struct thread *t, uint32_t init_flags) {
    _assert(t);

    // Setup default I/O context

    return 0;
}

int thread_init(
        struct thread *t,
        mm_space_t space,
        uintptr_t entry,
        uintptr_t rsp0_base,
        size_t rsp0_size,
        uintptr_t rsp3_base,
        size_t rsp3_size,
        uint32_t flags,
        uint32_t init_flags,
        void *arg) {
    void *kstack_sig;
    _assert(space);

    t->name[0] = 0;

    // 1. Allocate all the required structures
    t->data.data_flags = 0;

    if (!rsp0_base) {
        void *kstack = kmalloc(THREAD_KSTACK_SIZE);
        _assert(kstack);

        rsp0_base = (uintptr_t) kstack;
        rsp0_size = THREAD_KSTACK_SIZE;
    } else {
        t->data.data_flags |= THREAD_NOHEAP_STACK0;
    }

    if (!(flags & THREAD_KERNEL)) {
        if (!rsp3_base) {
            uintptr_t ustack = vmalloc(space, 0x100000, 0xF0000000, 4, VM_ALLOC_WRITE | VM_ALLOC_USER);
            _assert(ustack != MM_NADDR);

            rsp3_base = ustack;
            rsp3_size = 4 * 0x1000;
        } else {
            t->data.data_flags |= THREAD_NOHEAP_STACK3;
        }

        // TODO: alloc argp pages here

        kstack_sig = kmalloc(THREAD_KSTACK_SIZE);
        _assert(kstack_sig);
    }

    t->data.save_zone0 = (uintptr_t) kmalloc(FXSAVE_REGION);
    _assert(t->data.save_zone0);

    // 2. Assign allocated structure pointers to struct data
    t->data.stack0_base = rsp0_base;
    t->data.stack0_size = rsp0_size;
    if (!(flags & THREAD_KERNEL)) {
        t->data.stack3_base = rsp3_base;
        t->data.stack3_size = rsp3_size;
        t->data.stack0s_base = (uintptr_t) kstack_sig;
        t->data.stack0s_size = THREAD_KSTACK_SIZE;
    }

    t->data.rsp0 = t->data.stack0_base + t->data.stack0_size - sizeof(struct cpu_context);

    t->space = space;

    // Set this stuff to prevent undefined behavior
    t->sigq = 0;
    t->flags = flags;

    t->parent = NULL;
    t->child = NULL;
    t->next_child = NULL;

    t->next = NULL;

    memset(&t->ioctx, 0, sizeof(t->ioctx));
    memset(t->fds, 0, sizeof(t->fds));

    // 3. Setup platform context if requested
    if (init_flags & THREAD_INIT_CTX) {
        thread_platctx_init(t, entry, arg);
    }

    // 4. Setup I/O context if requested
    if (init_flags & THREAD_INIT_IOCTX) {
        thread_ioctx_init(t, init_flags);
    }

    return 0;
}

void thread_cleanup(struct thread *t) {
    _assert(t);

    // Release files
    for (size_t i = 0; i < 4; ++i) {
        if (t->fds[i].vnode) {
            //vfs_close(&t->ioctx, &t->fds[i]);
        }
    }

    // Release stacks
    if (!(t->data.data_flags & THREAD_NOHEAP_STACK0)) {
        kfree((void *) t->data.stack0_base);
    }
    // TODO: free stack3

    if (t->space != mm_kernel) {
        mm_space_free(t->space);
    }

    memset(t, 0, sizeof(struct thread));
    kfree(t);
}

void thread_set_name(struct thread *t, const char *name) {
    if (name) {
        _assert(strlen(name) < 32);
        strcpy(t->name, name);
    } else {
        t->name[0] = 0;
    }
}

// TODO: may be moved to platform-independent file
void thread_signal(struct thread *t, int s) {
    _assert(t);
    _assert(s > 0 && s <= 32);
    t->sigq |= (1 << (s - 1));
}

int sys_execve(const char *filename, const char *const argv[], const char *const envp[]) {
    panic("execve()\n");
}

int sys_fork(void) {
    panic("fork()\n");
}

void amd64_thread_set_ip(struct thread *t, uintptr_t ip) {
    struct cpu_context *ctx = (struct cpu_context *) t->data.rsp0;

    ctx->rip = ip;
    ctx->rflags = 0x248;
}

void amd64_thread_sigenter(struct thread *t, int signum) {
    _assert(t);

    // Swap contexts
    t->data.rsp0s = t->data.rsp0;
    t->data.rsp0 = t->data.stack0s_base + t->data.stack0s_size - sizeof(struct cpu_context);

    // Setup signal handler context
    struct cpu_context *ctx = (struct cpu_context *) t->data.rsp0;

    ctx->rip = t->sigentry;
    ctx->rflags = 0x248;

    if (t->flags & THREAD_KERNEL) {
        panic("TODO: kernel thread signal handling\n");
    } else {
        // Give one page of user stack for signal handling
        t->sigstack = t->data.stack3_base + 4096;
        ctx->rsp = t->sigstack;
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
    ctx->rdi = (uintptr_t) signum;

    ctx->cr3 = ({ _assert((uintptr_t) t->space >= 0xFFFFFF0000000000); (uintptr_t) t->space - 0xFFFFFF0000000000; });

    ctx->r8 = 0;
    ctx->r9 = 0;
    ctx->r10 = 0;
    ctx->r11 = 0;
    ctx->r12 = 0;
    ctx->r13 = 0;
    ctx->r14 = 0;
    ctx->r15 = 0;

    ctx->__canary = AMD64_STACK_CTX_CANARY;

    if (t == get_cpu()->thread) {
        panic("NYI\n");
    }
}

void amd64_thread_sigret(struct thread *t) {
    _assert(t);

    t->data.rsp0 = t->data.rsp0s;
}
