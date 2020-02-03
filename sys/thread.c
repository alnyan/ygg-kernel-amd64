#include "sys/amd64/hw/timer.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/pool.h"
#include "sys/amd64/context.h"
#include "sys/amd64/mm/map.h"
#include "sys/user/signum.h"
#include "sys/user/fcntl.h"
#include "sys/binfmt_elf.h"
#include "sys/user/errno.h"
#include "sys/amd64/cpu.h"
#include "sys/fs/ofile.h"
#include "sys/sys_proc.h"
#include "sys/vmalloc.h"
#include "sys/fs/vfs.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "sys/heap.h"
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

static struct thread *threads_all_head = NULL;
static pid_t last_kernel_pid = 0;
static pid_t last_user_pid = 0;

pid_t thread_alloc_pid(int is_user) {
    if (is_user) {
        return ++last_user_pid;
    } else {
        return -(++last_kernel_pid);
    }
}

static void thread_add(struct thread *thr) {
    if (threads_all_head) {
        threads_all_head->g_prev = thr;
    }
    thr->g_next = threads_all_head;
    thr->g_prev = NULL;
    threads_all_head = thr;
}

static void thread_remove(struct thread *thr) {
    struct thread *prev = thr->g_prev;
    struct thread *next = thr->g_next;

    if (prev) {
        prev->g_next = next;
    } else {
        threads_all_head = next;
    }

    if (next) {
        next->g_prev = prev;
    }

    thr->g_next = NULL;
    thr->g_prev = NULL;
}

////

static void thread_ioctx_empty(struct thread *thr) {
    memset(&thr->ioctx, 0, sizeof(struct vfs_ioctx));
    memset(thr->fds, 0, sizeof(thr->fds));
}

void thread_ioctx_fork(struct thread *dst, struct thread *src) {
    thread_ioctx_empty(dst);

    // TODO: increase refcount (when cwd has one)
    dst->ioctx.cwd_vnode = src->ioctx.cwd_vnode;
    dst->ioctx.gid = src->ioctx.gid;
    dst->ioctx.uid = src->ioctx.uid;

    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (src->fds[i]) {
            dst->fds[i] = src->fds[i];
            ++dst->fds[i]->refcount;
        }
    }
}

////

int thread_signal_pgid(pid_t pgid, int signum) {
    int ret = 0;

    for (struct thread *thr = threads_all_head; thr; thr = thr->g_next) {
        if (thr->state != THREAD_STOPPED && thr->pgid == pgid) {
            thread_signal(thr, signum);
            ++ret;
        }
    }

    return ret == 0 ? -1 : ret;
}

struct thread *thread_find(pid_t pid) {
    for (struct thread *thr = threads_all_head; thr; thr = thr->g_next) {
        if (thr->pid == pid) {
            return thr;
        }
    }
    return NULL;
}

struct thread *thread_child(struct thread *of, pid_t pid) {
    for (struct thread *thr = of->first_child; thr; thr = thr->next_child) {
        if (thr->pid == pid) {
            return thr;
        }
    }
    return NULL;
}

void thread_unchild(struct thread *thr) {
    struct thread *par = thr->parent;
    _assert(par);

    struct thread *p = NULL;
    struct thread *c = par->first_child;
    int found = 0;

    while (c) {
        if (c == thr) {
            found = 1;
            if (p) {
                p->next_child = thr->next_child;
            } else {
                par->first_child = thr->next_child;
            }
            break;
        }

        p = c;
        c = c->next_child;
    }

    _assert(found);
}

void thread_cleanup(struct thread *thr) {
    _assert(thr);
    // Leave only the system context required for hierachy tracking and error code/pid
    thr->state = THREAD_STOPPED;
    thr->flags |= THREAD_EMPTY;
    kinfo("Cleaning up %d\n", thr->pid);
    for (size_t i = 0; i < THREAD_MAX_FDS; ++i) {
        if (thr->fds[i]) {
            vfs_close(&thr->ioctx, thr->fds[i]);

            _assert(thr->fds[i]->refcount >= 0);
            if (thr->fds[i]->refcount == 0) {
                kfree(thr->fds[i]);
            }

            thr->fds[i] = NULL;
        }
    }

    // Release userspace pages
    mm_space_release(thr->space);
}

void thread_free(struct thread *thr) {
    _assert(thr->flags & THREAD_STOPPED);
    _assert(thr->flags & THREAD_EMPTY);

    // Free kstack
    for (size_t i = 0; i < thr->data.rsp0_size / MM_PAGE_SIZE; ++i) {
        amd64_phys_free(MM_PHYS(i * MM_PAGE_SIZE + thr->data.rsp0_base));
    }

    // Free page directory (if not mm_kernel)
    if (thr->space != mm_kernel) {
        // Make sure we don't shoot a leg off
        uintptr_t cr3;
        asm volatile ("movq %%cr3, %0":"=a"(cr3));
        _assert(MM_VIRTUALIZE(cr3) != (uintptr_t) thr->space);

        mm_space_free(thr->space);
    }

    // Free thread itself
    memset(thr, 0, sizeof(struct thread));
    kfree(thr);
}

int thread_init(struct thread *thr, uintptr_t entry, void *arg, int user) {
    uintptr_t stack_pages = amd64_phys_alloc_contiguous(2);
    _assert(stack_pages != MM_NADDR);

    thr->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    thr->data.rsp0_size = MM_PAGE_SIZE * 2;
    thr->data.rsp0_top = thr->data.rsp0_base + thr->data.rsp0_size;
    thr->name[0] = 0;
    thr->flags = user ? 0 : THREAD_KERNEL;

    uint64_t *stack = (uint64_t *) (thr->data.rsp0_base + thr->data.rsp0_size);

    if (user) {
        mm_space_t space = amd64_mm_pool_alloc();
        mm_space_clone(space, mm_kernel, MM_CLONE_FLG_KERNEL);
        thr->data.cr3 = MM_PHYS(space);
        thr->space = space;

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
        thr->space = mm_kernel;
    }

    thr->state = THREAD_READY;
    thr->parent = NULL;
    thr->first_child = NULL;
    thr->next_child = NULL;

    thread_ioctx_empty(thr);

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
    thr->sigq = 0;
    thr->pgid = -1;
    thr->pid = -1;

    thread_add(thr);

    return 0;
}

// TODO: support kthread forking()
//       (Although I don't really think it's very useful -
//        threads can just be created by thread_init() and
//        sched_queue())
int sys_fork(struct sys_fork_frame *frame) {
    struct thread *dst = kmalloc(sizeof(struct thread));
    _assert(dst);
    struct thread *src = thread_self;

    uintptr_t stack_pages = amd64_phys_alloc_contiguous(2);
    _assert(stack_pages != MM_NADDR);

    dst->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    dst->data.rsp0_size = MM_PAGE_SIZE * 2;
    dst->data.rsp0_top = dst->data.rsp0_base + dst->data.rsp0_size;
    dst->flags = 0;

    mm_space_t space = amd64_mm_pool_alloc();
    mm_space_fork(space, src->space, MM_CLONE_FLG_KERNEL | MM_CLONE_FLG_USER);

    dst->data.rsp3_base = src->data.rsp3_base;
    dst->data.rsp3_size = src->data.rsp3_size;

    dst->data.cr3 = MM_PHYS(space);
    dst->space = space;

    dst->signal_entry = src->signal_entry;

    thread_ioctx_fork(dst, src);

    dst->state = THREAD_READY;
    dst->parent = src;
    dst->next_child = src->first_child;
    src->first_child = dst;
    dst->first_child = NULL;

    uint64_t *stack = (uint64_t *) dst->data.rsp0_top;

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
    dst->pgid = src->pgid;
    dst->sigq = 0;

    thread_add(dst);
    sched_queue(dst);

    return dst->pid;
}

uintptr_t argp_copy(struct thread *thr, const char *const argv[], size_t *arg_count) {
    uintptr_t dst_page_phys = amd64_phys_alloc_page();
    if (dst_page_phys == MM_NADDR) {
        return dst_page_phys;
    }

    char *dst_page = (char *) MM_VIRTUALIZE(dst_page_phys);

    // Count the arguments
    size_t argc = 0;
    while (1) {
        if (!argv[argc]) {
            break;
        }
        ++argc;
    }
    *arg_count = argc;

    // Argp page layout:
    // ptr0 ptr1 ptr2 ... ptrN NULL str0 str1 str2 ... strN
    size_t data_offset = (argc + 1) * sizeof(uintptr_t);
    size_t str_offset = 0;

    // Copy string data
    for (size_t i = 0; i < argc; ++i) {
        size_t len = strlen(argv[i]);

        if (len + str_offset + data_offset >= 4096) {
            panic("Argument list too large\n");
        }

        // Setup pointer to new offset
        ((uintptr_t *) dst_page)[i] = str_offset + data_offset;

        // Copy data to that offset
        strcpy(&dst_page[data_offset + str_offset], argv[i]);
        str_offset += len + 1;
    }

    // Setup last entry
    ((uintptr_t *) dst_page)[argc] = 0;

    return dst_page_phys;
}

int sys_execve(const char *path, const char **argv, const char **envp) {
    struct thread *thr = thread_self;
    _assert(thr);
    struct ofile fd;
    struct stat st;
    uintptr_t entry;
    size_t argc;
    int res;

    if ((res = vfs_stat(&thr->ioctx, path, &st)) != 0) {
        kerror("execve(%s): %s\n", path, kstrerror(res));
        return res;
    }

    const char *e = strrchr(path, '/');
    const char *name = e + 1;
    if (!e) {
        name = path;
    }
    size_t name_len = MIN(strlen(name), sizeof(thr->name) - 1);
    strncpy(thr->name, name, name_len);
    thr->name[name_len] = 0;

    // Copy args
    _assert(argv);
    uintptr_t argp_phys = argp_copy(thr, argv, &argc);
    _assert(argp_phys != MM_NADDR);

    if ((res = vfs_open(&thr->ioctx, &fd, path, O_RDONLY, 0)) != 0) {
        kerror("%s: %s\n", path, kstrerror(res));
        return res;
    }

    if (thr->space == mm_kernel) {
        // Have to allocate a new PID for kernel -> userspace transition
        thr->pid = thread_alloc_pid(1);
        thr->pgid = thr->pid;

        // Have to remove parent/child relation for transition
        _assert(!thr->first_child);
        if (thr->parent) {
            panic("NYI\n");
        }
        thr->first_child = NULL;
        thr->next_child = NULL;
        thr->parent = NULL;
        thr->sigq = 0;

        thr->space = amd64_mm_pool_alloc();
        _assert(thr->space);
        thr->data.cr3 = MM_PHYS(thr->space);
        thr->flags = 0;

        mm_space_clone(thr->space, mm_kernel, MM_CLONE_FLG_KERNEL);
    } else {
        mm_space_release(thr->space);
    }

    if ((res = elf_load(thr, &thr->ioctx, &fd, &entry)) != 0) {
        vfs_close(&thr->ioctx, &fd);

        kerror("elf load failed: %s\n", kstrerror(res));
        sys_exit(-1);

        panic("This code shouldn't run\n");
    }

    vfs_close(&thr->ioctx, &fd);

    // Allocate a virtual address to map argp page
    uintptr_t argp_virt = vmfind(thr->space, 0x100000, 0xF0000000, 1);
    _assert(argp_virt != MM_NADDR);
    // Map it as non-writable user-accessable
    _assert(amd64_map_single(thr->space, argp_virt, argp_phys, (1 << 2)) == 0);
    // Fix up the pointers
    uintptr_t *argp_fixup = (uintptr_t *) MM_VIRTUALIZE(argp_phys);
    for (size_t i = 0; i < argc; ++i) {
        argp_fixup[i] += argp_virt;
    }

    thr->signal_entry = 0;
    thr->data.rsp0 = thr->data.rsp0_top;

    // Allocate a new user stack
    uintptr_t ustack = vmalloc(thr->space, 0x100000, 0xF0000000, 4, MM_PAGE_USER | MM_PAGE_WRITE | MM_PAGE_NOEXEC);
    thr->data.rsp3_base = ustack;
    thr->data.rsp3_size = 4 * MM_PAGE_SIZE;

    context_exec_enter((void *) argp_virt, thr, ustack + 4 * MM_PAGE_SIZE, entry);

    panic("This code shouldn't run\n");
}

void thread_sigenter(int signum) {
    struct thread *thr = thread_self;
    kdebug("%d: Handle signal %d\n", thr->pid, signum);
    uintptr_t old_rsp0_top = thr->data.rsp0_top;
    // XXX: Either use a separate stack or ensure stuff doesn't get overwritten
    uintptr_t signal_rsp3 = thr->data.rsp3_base + 0x800;

    context_sigenter(thr->signal_entry, signal_rsp3, signum);

    thr->data.rsp0_top = old_rsp0_top;
}

__attribute__((noreturn)) void sys_exit(int status) {
    struct thread *thr = thread_self;
    kdebug("Thread %d exited with status %d\n", thr->pid, status);

    thr->exit_status = status;

    if (thr->parent) {
        thread_signal(thr->parent, SIGCHLD);
    }

    // Sure that no code of this thread will be running anymore -
    // can clean up its stuff
    thread_cleanup(thr);

    sched_unqueue(thr, THREAD_STOPPED);
    panic("This code shouldn't run\n");
}

int sys_waitpid(pid_t pid, int *status) {
    struct thread *thr = thread_self;
    _assert(thr);
    struct thread *chld = thread_child(thr, pid);

    if (!chld) {
        return -ECHILD;
    }

    while (chld->state != THREAD_STOPPED) {
        sched_unqueue(thr, THREAD_WAITING_PID);
        if (thread_signal_pending(thr, SIGCHLD)) {
            thread_signal_clear(thr, SIGCHLD);
            break;
        }
        // Handle any other pending signal
        kdebug("Waken up by some other signal, continuing\n");
        thread_check_signal(thr, 0);
    }

    // Make sure no platform context remains
    _assert(chld->flags & THREAD_EMPTY);

    if (status) {
        *status = chld->exit_status;
    }

    thread_unchild(chld);
    thread_remove(chld);
    thread_free(chld);

    return 0;
}

int thread_sleep(struct thread *thr, uint64_t deadline, uint64_t *int_time) {
    thr->sleep_deadline = deadline;
    timer_add_sleep(thr);
    sched_unqueue(thr, THREAD_WAITING);
    // Store time when interrupt occured
    if (int_time) {
        *int_time = system_time;
    }
    return thread_check_signal(thr, 0);
}

void sys_sigreturn(void) {
    context_sigreturn();
}

void thread_signal(struct thread *thr, int signum) {
    if (thr == thread_self) {
        kdebug("Signal will be handled now\n");
        thread_sigenter(signum);
    } else {
        kdebug("Signal will be handled later\n");
        thread_signal_set(thr, signum);

        if (thr->state == THREAD_WAITING) {
            timer_remove_sleep(thr);
        }

        sched_queue(thr);
    }
}

int thread_check_signal(struct thread *thr, int ret) {
    if (thr->sigq) {
        // Pick one signal to handle at a time
        int signum = 0;
        for (int i = 0; i < 64; ++i) {
            if (thr->sigq & (1ULL << i)) {
                thr->sigq &= ~(1ULL << i);
                signum = i + 1;
                break;
            }
        }
        _assert(signum);
        thread_sigenter(signum);

        // Theoretically, a rogue thread could steal all the CPU time by sending itself signals
        // in normal context, as after returning from thread_sigenter() this code will return
        // to a normal execution
        // XXX: Maybe makes sense to just yield() here
        return -EINTR;
    }

    return ret;
}

int sys_kill(pid_t pid, int signum) {
    struct thread *thr;

    if (pid > 0) {
        thr = thread_find(pid);
    } else if (pid == 0) {
        thr = thread_self;
    } else {
        // Not implemented
        thr = NULL;
    }

    if (!thr || thr->state == THREAD_STOPPED) {
        return -ESRCH;
    }

    if (signum == 0) {
        return 0;
    }

    if (signum <= 0 || signum >= 64) {
        return -EINVAL;
    }

    if (!thr) {
        // No such process
        return -ESRCH;
    }

    thread_signal(thr, signum);

    return 0;
}

void sys_sigentry(uintptr_t entry) {
    struct thread *thr = thread_self;
    _assert(thr);

    thr->signal_entry = entry;
}

pid_t sys_getpid(void) {
    struct thread *thr = thread_self;
    _assert(thr);
    return thr->pid;
}

pid_t sys_getpgid(pid_t pid) {
    struct thread *thr;

    if (pid == 0) {
        thr = get_cpu()->thread;
        _assert(thr);
    } else {
        thr = thread_find(pid);
    }

    if (!thr) {
        return -ESRCH;
    }

    return thr->pgid;
}

int sys_setpgid(pid_t pid, pid_t pgrp) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (pid == 0 && pgrp == 0) {
        thr->pgid = thr->pid;
        return 0;
    }

    // Find child with pid pid (guess only children can be setpgid'd)
    struct thread *chld = thread_child(thr, pid);
    if (!chld) {
        return -ESRCH;
    }
    if (chld->pgid != thr->pgid) {
        return -EACCES;
    }
    chld->pgid = pgrp;

    return 0;
}

int sys_setuid(uid_t uid) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (thr->ioctx.uid != 0) {
        return -EACCES;
    }

    thr->ioctx.uid = uid;
    return 0;
}

int sys_setgid(gid_t gid) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (thr->ioctx.gid != 0 && thr->ioctx.uid != 0) {
        return -EACCES;
    }

    thr->ioctx.gid = gid;
    return 0;
}

uid_t sys_getuid(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return thr->ioctx.uid;
}

gid_t sys_getgid(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return thr->ioctx.gid;
}

int sys_brk(void *addr) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    uintptr_t image_end = thr->image_end;
    uintptr_t curbrk = thr->brk;

    if ((uintptr_t) addr < curbrk) {
        panic("TODO: negative sbrk\n");
    }

    if ((uintptr_t) addr >= 0x100000000) {
        // We don't like addr
        return -EINVAL;
    }

    // Map new pages in brk area
    uintptr_t addr_page_align = image_end & ~0xFFF;
    size_t brk_size = (uintptr_t) addr - image_end;
    size_t npages = (brk_size + 0xFFF) / 0x1000;
    uintptr_t page_phys;

    kdebug("brk uses %u pages from %p\n", npages, addr_page_align);

    for (size_t i = 0; i < npages; ++i) {
        if ((page_phys = amd64_map_get(thr->space, addr_page_align + (i << 12), NULL)) == MM_NADDR) {
            // Allocate a page here
            page_phys = amd64_phys_alloc_page();
            if (page_phys == MM_NADDR) {
                return -ENOMEM;
            }

            kdebug("%p: allocated a page\n", addr_page_align + (i << 12));

            if (amd64_map_single(thr->space, addr_page_align + (i << 12), page_phys, (1 << 1) | (1 << 2)) != 0) {
                amd64_phys_free(page_phys);
                return -ENOMEM;
            }
        } else {
            kdebug("%p: already present\n", addr_page_align + (i << 12));
        }
    }

    thr->brk = (uintptr_t) addr;

    return 0;
}
