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
    t->data.data_flags = 0;

    if (!rsp0_base) {
        void *kstack = kmalloc(THREAD_KSTACK_SIZE);
        _assert(kstack);

        rsp0_base = (uintptr_t) kstack;
        rsp0_size = THREAD_KSTACK_SIZE;
    } else {
        t->data.data_flags |= 1 /* Don't free kstack */;
    }

    if (!(flags & THREAD_KERNEL) && !rsp3_base) {
        uintptr_t ustack = vmalloc(space, 0x100000, 0xF0000000, 4, VM_ALLOC_USER | VM_ALLOC_WRITE);
        _assert(ustack != MM_NADDR);

        rsp3_base = ustack;
        rsp3_size = 4 * 0x1000;

        kdebug("Allocated user stack: %p\n", ustack);
    } else if (rsp3_base) {
        t->data.data_flags |= 2 /* Don't free ustack */;
    }

    if (!(flags & THREAD_KERNEL)) {
        _assert((t->argp_page_phys = amd64_phys_alloc_page()) != MM_NADDR);
        t->argp_page = 0x100000000;
        _assert(!amd64_map_single(space, t->argp_page, t->argp_page_phys, 1 | (1 << 1) | (1 << 2)));
    } else {
        t->argp_page = MM_NADDR;
    }

    t->data.stack0_base = rsp0_base;
    t->data.stack0_size = rsp0_size;
    t->data.rsp0 = t->data.stack0_base + t->data.stack0_size - sizeof(struct cpu_context);

    if (!(flags & THREAD_KERNEL)) {
        t->data.stack3_base = rsp3_base;
        t->data.stack3_size = rsp3_size;
    }

    // Alloc signal handling kstack
    void *kstack_sig = kmalloc(THREAD_KSTACK_SIZE);
    _assert(kstack_sig);

    t->data.stack0s_base = (uintptr_t) kstack_sig;
    t->data.stack0s_size = THREAD_KSTACK_SIZE;

    // No need to setup context for signal handler stack as it will be set up on
    // signal entry

    // Setup normal context
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

    if (!(flags & THREAD_CTX_ONLY)) {
        t->space = space;
        t->flags = flags;
        t->next = NULL;
        t->sigq = 0;

        memset(&t->ioctx, 0, sizeof(t->ioctx));
        memset(t->fds, 0, sizeof(t->fds));
    }

    return 0;
}

void thread_cleanup(struct thread *t) {
    _assert(t);

    // Release files
    for (size_t i = 0; i < 4; ++i) {
        if (t->fds[i].vnode) {
            vfs_close(&t->ioctx, &t->fds[i]);
        }
    }

    // Release stacks
    if (!(t->data.data_flags & 1)) {
        kfree((void *) t->data.stack0_base);
    }

    if (t->space != mm_kernel) {
        mm_space_free(t->space);
    }

    memset(t, 0, sizeof(struct thread));
    kfree(t);
}

// TODO: may be moved to platform-independent file
void thread_signal(struct thread *t, int s) {
    _assert(t);
    _assert(s > 0 && s <= 32);
    t->sigq |= (1 << (s - 1));
}

int sys_execve(const char *filename, const char *const argv[], const char *const envp[]) {
    asm volatile ("cli");
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    // I know it's slow and ugly
    char *file_buf;
    struct stat st;
    struct ofile fd;
    size_t pos = 0;
    ssize_t bread;
    int res;

    if ((res = vfs_stat(&thr->ioctx, filename, &st)) < 0) {
        return res;
    }

    if ((st.st_mode & S_IFMT) != S_IFREG) {
        return -ENOEXEC;
    }

    if (!(st.st_mode & 0111)) {
        return -ENOEXEC;
    }

    file_buf = kmalloc(st.st_size);
    if (!file_buf) {
        return -ENOMEM;
    }

    if ((res = vfs_open(&thr->ioctx, &fd, filename, 0, O_RDONLY)) != 0) {
        return res;
    }

    while ((bread = vfs_read(&thr->ioctx, &fd, (char *) file_buf + pos, MIN(512, st.st_size - pos))) > 0) {
        pos += bread;
    }

    vfs_close(&thr->ioctx, &fd);

    mm_space_t space = thr->space;

    // Buffer page used for argp construction
    // Before we released the old memory, copy
    // the needed stuff from it
    uintptr_t argp_page_phys = amd64_phys_alloc_page();
    _assert(argp_page_phys != MM_NADDR);
    char **dst_argp = (char **) MM_VIRTUALIZE(argp_page_phys);
    size_t argp_offset = 0, argp_offset2 = 0;
    size_t argc = 0;
    if (argv) {
        while (argv[argc]) {
            dst_argp[argc] = (char *) argp_offset2;
            argp_offset2 += strlen(argv[argc]) + 1;
            if (argp_offset2 >= 4096) {
                panic("argp buffer overflow\n");
            }
            ++argc;
        }
        dst_argp[argc] = NULL;
        argp_offset = (size_t) &dst_argp[argc + 1];
        // Convert offsets to pointers
        for (size_t i = 0; i < argc; ++i) {
            char *ent = dst_argp[i] + argp_offset;
            dst_argp[i] = (char *) (MM_PHYS(ent) - argp_page_phys);
            strcpy(ent, argv[i]);
            kdebug("Cloning argument: %s -> %p\n", ent, dst_argp[i]);
        }
    } else {
        dst_argp[0] = NULL;
    }

    // 1. Discard memory
    // XXX: And this is why I need CoW
    mm_space_release(space);

    // 2. Reinitialize context
    _assert(thread_init(thr,
                        space,
                        0,
                        thr->data.stack0_base,
                        thr->data.stack0_size,
                        0,
                        0,
                        THREAD_CTX_ONLY,
                        NULL
            ) == 0);
    // Revirtualize offsets
    for (size_t i = 0; i < argc; ++i) {
        dst_argp[i] = (char *) ((uintptr_t) dst_argp[i] + thr->argp_page);
    }
    // Copy the data block
    memcpy((void *) MM_VIRTUALIZE(thr->argp_page_phys), dst_argp, 4096);
    amd64_phys_free(argp_page_phys);
    for (size_t i = 0; i < argc; ++i) {
        kdebug("%p[%d] = %p\n", thr->argp_page, i, ((char **) MM_VIRTUALIZE(thr->argp_page_phys))[i]);
    }
    kdebug("argp buffer = %p\n", thr->argp_page);

    struct cpu_context *ctx = (struct cpu_context *) thr->data.rsp0;
    ctx->rdi = thr->argp_page;

    // 3. Load new binary
    _assert(elf_load(thr, file_buf) == 0);
    kfree(file_buf);

    amd64_syscall_iretq(ctx);

    panic("This code should not execute\n");
    return -1;
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
    void *dst_kstack_sig = kmalloc(THREAD_KSTACK_SIZE);
    struct amd64_thread *dst_data = &thr_dst->data;
    _assert(dst_kstack && dst_kstack_sig);

    dst_data->data_flags |= 1 /* Don't free kstack */;
    dst_data->stack0_base = (uintptr_t) dst_kstack;
    dst_data->stack0_size = THREAD_KSTACK_SIZE;
    dst_data->stack0s_base = (uintptr_t) dst_kstack_sig;
    dst_data->stack0s_size = THREAD_KSTACK_SIZE;
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

    if ((res = vfs_open(&thr_dst->ioctx, &thr_dst->fds[0], "/dev/tty0", O_RDONLY, 0)) < 0) {
        panic("Failed to set up tty0 for input: %s\n", kstrerror(res));
    }
    if ((res = vfs_open(&thr_dst->ioctx, &thr_dst->fds[1], "/dev/tty0", O_WRONLY, 0)) < 0) {
        panic("Failed to set up tty0 for output: %s\n", kstrerror(res));
    }

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
