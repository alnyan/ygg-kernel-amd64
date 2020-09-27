#include "arch/amd64/context.h"
#include "arch/amd64/mm/pool.h"
#include "sys/snprintf.h"
#include "sys/mem/phys.h"
#include "sys/thread.h"
#include "sys/string.h"
#include "user/errno.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "fs/sysfs.h"
#include "sys/heap.h"
#include "fs/ofile.h"

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

LIST_HEAD(proc_all_head);
static pid_t last_kernel_pid = 0;
static pid_t last_user_pid = 0;
static struct vnode *g_sysfs_proc_dir;

static int sysfs_proc_name(void *ctx, char *buf, size_t lim) {
    sysfs_buf_printf(buf, lim, "%s\n", ((struct process *) ctx)->name);
    return 0;
}

static int sysfs_proc_parent(void *ctx, char *buf, size_t lim) {
    struct process *proc = ctx;
    if (!proc->parent) {
        sysfs_buf_printf(buf, lim, "1\n");
    } else {
        sysfs_buf_printf(buf, lim, "%d\n", proc->parent->pid);
    }
    return 0;
}

static int sysfs_proc_ioctx(void *ctx, char *buf, size_t lim) {
    struct process *proc = ctx;
    sysfs_buf_printf(buf, lim, "%d:%d\n", proc->ioctx.uid, proc->ioctx.gid);
    return 0;
}

static struct vnode *sysfs_proc_self(struct thread *ctx, struct vnode *link, char *buf, size_t lim) {
    _assert(ctx && ctx->proc);
    struct vnode *res = ctx->proc->fs_entry;

    if (buf && strlen(res->name) < lim) {
        if (strlen(res->name) < lim) {
            strcpy(buf, res->name);
        } else {
            return NULL;
        }
    }
    return ctx->proc->fs_entry;
}

static void proc_ensure_dir(void) {
    if (!g_sysfs_proc_dir) {
        _assert(sysfs_add_dir(NULL, "proc", &g_sysfs_proc_dir) == 0);

        struct vnode *vn = vnode_create(VN_LNK, "self");
        vn->flags = VN_MEMORY | VN_PER_PROCESS;
        vn->target_func = sysfs_proc_self;
        vnode_attach(g_sysfs_proc_dir, vn);
    }
}

void proc_add_entry(struct process *proc) {
    char name[16];

    proc_ensure_dir();

    snprintf(name, sizeof(name), "%d", proc->pid);
    _assert(sysfs_add_dir(g_sysfs_proc_dir, name, &proc->fs_entry) == 0);

    _assert(sysfs_add_config_endpoint(proc->fs_entry, "name", SYSFS_MODE_DEFAULT, 64,
                                      proc, sysfs_proc_name, NULL) == 0);
    if (proc->pid > 0) {
        _assert(sysfs_add_config_endpoint(proc->fs_entry, "parent", SYSFS_MODE_DEFAULT, 64,
                                          proc, sysfs_proc_parent, NULL) == 0);
        _assert(sysfs_add_config_endpoint(proc->fs_entry, "ioctx", SYSFS_MODE_DEFAULT, 64,
                                          proc, sysfs_proc_ioctx, NULL) == 0);
    }
}

void proc_del_entry(struct process *proc) {
    sysfs_del_ent(proc->fs_entry);
}

pid_t process_alloc_pid(int is_user) {
    if (is_user) {
        return ++last_user_pid;
    } else {
        return -(++last_kernel_pid);
    }
}



static void process_ioctx_empty(struct process *proc) {
    memset(&proc->ioctx, 0, sizeof(struct vfs_ioctx));
    memset(proc->fds, 0, sizeof(proc->fds));
    proc->ioctx.umask = 0022;
}

void process_ioctx_fork(struct process *dst, struct process *src) {
    process_ioctx_empty(dst);

    dst->ioctx.cwd_vnode = src->ioctx.cwd_vnode;
    dst->ioctx.gid = src->ioctx.gid;
    dst->ioctx.uid = src->ioctx.uid;
    dst->ioctx.umask = src->ioctx.umask;

    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (src->fds[i]) {
            dst->fds[i] = ofile_dup(src->fds[i]);
        }
    }
}

int process_signal_pgid(pid_t pgid, int signum) {
    int ret = 0;

    struct process *proc;
    list_for_each_entry(proc, &proc_all_head, g_link) {
        if (proc->proc_state != PROC_FINISHED && proc->pgid == pgid) {
            process_signal(proc, signum);
            ++ret;
        }
    }

    return ret == 0 ? -ECHILD : ret;
}

int process_signal_children(struct process *proc, int signum) {
    int ret = 0;
    for (struct process *chld = proc->first_child; chld; chld = chld->next_child) {
        if (proc->proc_state != PROC_FINISHED) {
            process_signal(chld, signum);
            ++ret;
        }
    }
    return ret == 0 ? -ECHILD : 0;
}

struct process *process_find(pid_t pid) {
    struct process *proc;
    list_for_each_entry(proc, &proc_all_head, g_link) {
        if (proc->pid == pid) {
            return proc;
        }
    }

    return NULL;
}

struct thread *process_first_thread(struct process *proc) {
    _assert(proc);
    _assert(!list_empty(&proc->thread_list) && proc->thread_count);
    return list_first_entry(&proc->thread_list, struct thread, thread_link);
}

struct process *process_child(struct process *of, pid_t pid) {
    for (struct process *proc = of->first_child; proc; proc = proc->next_child) {
        if (proc->pid == pid) {
            return proc;
        }
    }
    return NULL;
}

void process_unchild(struct process *proc) {
    struct process *par = proc->parent;
    _assert(par);

    struct process *p = NULL;
    struct process *c = par->first_child;
    int found = 0;

    while (c) {
        if (c == proc) {
            found = 1;
            if (p) {
                p->next_child = proc->next_child;
            } else {
                par->first_child = proc->next_child;
            }
            break;
        }

        p = c;
        c = c->next_child;
    }

    _assert(found);
}

void process_cleanup(struct process *proc) {
    // Leave only the system context required for hierachy tracking and error code/pid
    proc_del_entry(proc);

    // Reparent all children to init (1)
    struct process *chld = proc->first_child, *chld_next;
    struct process *init;
    while (chld) {
        init = process_find(1);
        _assert(init);

        chld_next = chld->next_child;
        chld->parent = init;
        chld->next_child = init->first_child;
        init->first_child = chld;

        chld = chld_next;
    }
    proc->first_child = NULL;

    _assert(proc->proc_state == PROC_FINISHED);
    _assert(proc->thread_count == 1);
    proc->flags |= PROC_EMPTY;
    kdebug("Cleaning up %d\n", proc->pid);
    // Release userspace pages
    mm_space_release(proc);
}

void process_free(struct process *proc) {
    // Make sure all the threads of the process have stopped -
    // only main remains
    _assert(proc->thread_count == 1);

    // Sure that no code of this thread will be running anymore -
    // can clean up its stuff
    process_cleanup(proc);

    _assert(proc->proc_state == PROC_FINISHED);
    struct thread *thr;
    thr = list_first_entry(&proc->thread_list, struct thread, thread_link);
    _assert(thr);

    // Free kstack
    for (size_t i = 0; i < thr->data.rsp0_size / MM_PAGE_SIZE; ++i) {
        mm_phys_free_page(MM_PHYS(i * MM_PAGE_SIZE + thr->data.rsp0_base));
    }

    // Free page directory (if not mm_kernel)
    if (proc->space != mm_kernel) {
        // Make sure we don't shoot a leg off
        uintptr_t cr3;
        asm volatile ("movq %%cr3, %0":"=a"(cr3));
        _assert(MM_VIRTUALIZE(cr3) != (uintptr_t) proc->space);

        mm_space_free(proc);
    }

    kfree(thr->data.fxsave);

    // Free thread itself
    memset(thr, 0, sizeof(struct thread));
    kfree(thr);
    // Free the process
    memset(proc, 0, sizeof(struct process));
    kfree(proc);
}

int process_init_thread(struct process *proc, uintptr_t entry, void *arg, int user) {
    list_head_init(&proc->g_link);
    list_head_init(&proc->shm_list);
    thread_wait_io_init(&proc->pid_notify);

    proc->name[0] = 0;
    proc->flags = user ? 0 : THREAD_KERNEL;

    if (user) {
        proc->space = amd64_mm_pool_alloc();
        mm_space_clone(proc->space, mm_kernel, MM_CLONE_FLG_KERNEL);
    } else {
        proc->space = mm_kernel;
    }

    struct thread *main_thread = kmalloc(sizeof(struct thread));
    _assert(main_thread);
    main_thread->proc = proc;
    list_head_init(&proc->thread_list);

    int res = thread_init(main_thread, entry, arg, user ? THR_INIT_USER : 0);
    _assert(res == 0);

    process_ioctx_empty(proc);

    list_add(&main_thread->thread_link, &proc->thread_list);
    proc->thread_count = 1;
    proc->parent = NULL;
    proc->first_child = NULL;
    proc->next_child = NULL;
    proc->pgid = -1;
    proc->pid = process_alloc_pid(user);
    proc->ctty = NULL;
    kdebug("New process #%d with main thread <%p>\n", proc->pid, main_thread);

    proc->sigq = 0;
    proc->proc_state = PROC_ACTIVE;

    list_add(&proc->g_link, &proc_all_head);

    proc_add_entry(proc);

    return 0;
}

// TODO: share more parts with thread_init()
// TODO: support kthread forking()
//       (Although I don't really think it's very useful -
//        threads can just be created by thread_init() and
//        sched_queue())
int sys_fork(struct sys_fork_frame *frame) {
    struct thread *src_thread = thread_self;
    _assert(src_thread);
    struct process *src = src_thread->proc;
    _assert(src);

    if (src->flags & THREAD_KERNEL) {
        panic("XXX: kthread fork()ing not implemented\n");
    }

    if (src->thread_count != 1) {
        panic("XXX: fork() a multithreaded process\n");
    }

    struct process *dst = kmalloc(sizeof(struct process));
    _assert(dst);
    list_head_init(&dst->thread_list);
    struct thread *dst_thread = kmalloc(sizeof(struct thread));
    _assert(dst_thread);
    list_head_init(&dst_thread->thread_link);

    // Tie the two together
    list_add(&dst_thread->thread_link, &dst->thread_list);
    dst->thread_count = 1;
    dst_thread->proc = dst;

    // Initialize dst process: memory space
    mm_space_t space = amd64_mm_pool_alloc();
    dst->space = space;
    mm_space_fork(dst, src, MM_CLONE_FLG_KERNEL | MM_CLONE_FLG_USER);

    // Initialize dst process state
    list_head_init(&dst->g_link);
    list_head_init(&dst->shm_list);
    thread_wait_io_init(&dst->pid_notify);
    dst->flags = 0;
    strcpy(dst->name, src->name);
    process_ioctx_fork(dst, src);
    dst->parent = src;
    dst->next_child = src->first_child;
    src->first_child = dst;
    dst->first_child = NULL;
    dst->pid = process_alloc_pid(1);
    dst->pgid = src->pgid;
    dst->sigq = 0;
    dst->proc_state = PROC_ACTIVE;
    dst->ctty = src->ctty;
    kdebug("New process #%d with main thread <%p>\n", dst->pid, dst_thread);

    // Initialize dst thread
    uintptr_t stack_pages = mm_phys_alloc_contiguous(THREAD_KSTACK_PAGES, PU_KERNEL);
    _assert(stack_pages != MM_NADDR);
    list_head_init(&dst_thread->wait_head);
    thread_wait_io_init(&dst_thread->sleep_notify);

    dst_thread->sched_prev = NULL;
    dst_thread->sched_next = NULL;

    dst_thread->data.rsp0_base = MM_VIRTUALIZE(stack_pages);
    dst_thread->data.rsp0_size = MM_PAGE_SIZE * THREAD_KSTACK_PAGES;
    dst_thread->data.rsp0_top = dst_thread->data.rsp0_base + dst_thread->data.rsp0_size;
    dst_thread->flags = 0;
    dst_thread->sigq = 0;

    dst_thread->data.rsp3_base = src_thread->data.rsp3_base;
    dst_thread->data.rsp3_size = src_thread->data.rsp3_size;

    dst_thread->signal_entry = src_thread->signal_entry;
    dst_thread->signal_stack_base = src_thread->signal_stack_base;
    dst_thread->signal_stack_size = src_thread->signal_stack_size;

    dst_thread->data.cr3 = MM_PHYS(space);

    dst_thread->data.fxsave = kmalloc(FXSAVE_REGION);
    _assert(dst_thread->data.fxsave);
    _assert(src_thread->data.fxsave);

    if (src_thread->flags & THREAD_FPU_SAVED) {
        memcpy(dst_thread->data.fxsave, src_thread->data.fxsave, FXSAVE_REGION);
    }

    dst_thread->state = THREAD_READY;

    uint64_t *stack = (uint64_t *) dst_thread->data.rsp0_top;

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

    dst_thread->data.rsp0 = (uintptr_t) stack;

    list_add(&dst->g_link, &proc_all_head);
    proc_add_entry(dst);
    sched_queue(dst_thread);

    return dst->pid;
}

void process_signal(struct process *proc, int signum) {
    // Check if it's a thread-only signal (exception)
    _assert(!signal_is_exception(signum));

    if (proc->proc_state == PROC_SUSPENDED) {
        // Can only KILL or CONT suspended process
        if (signum == SIGCONT) {
            proc->proc_state = PROC_ACTIVE;
        }
        if (signum != SIGKILL && signum != SIGCONT) {
            return;
        }
    }

    struct thread *thr;
    list_for_each_entry(thr, &proc->thread_list, thread_link) {
        if (thr->state != THREAD_STOPPED) {
            return thread_signal(thr, signum);
        }
    }

    panic("Failed to deliver the signal\n");
}

int sys_kill(pid_t pid, int signum) {
    struct process *proc;

    if (pid > 0) {
        proc = process_find(pid);
    } else if (pid == 0) {
        proc = thread_self->proc;
    } else if (pid < -1) {
        return process_signal_pgid(-pid, signum);
    } else if (pid == -1) {
        proc = thread_self->proc;
        return process_signal_children(proc, signum);
    } else {
        panic("Not implemented\n");
    }

    if (!proc || proc->proc_state == PROC_FINISHED) {
        return -ESRCH;
    }

    if (signum == 0) {
        return 0;
    }

    if (signum <= 0 || signum >= 64) {
        return -EINVAL;
    }

    if (!proc) {
        // No such process
        return -ESRCH;
    }

    process_signal(proc, signum);

    return 0;
}

pid_t sys_getpid(void) {
    _assert(thread_self && thread_self->proc);
    return thread_self->proc->pid;
}

pid_t sys_getppid(void) {
    _assert(thread_self && thread_self->proc);
    struct process *proc = thread_self->proc;
    if (!proc->parent) {
        _assert(proc->pid == 1);
        return 1;
    } else {
        return proc->parent->pid;
    }
}

pid_t sys_getpgid(pid_t pid) {
    struct process *proc;

    if (pid == 0) {
        proc = thread_self->proc;
    } else {
        proc = process_find(pid);
    }

    if (!proc) {
        return -ESRCH;
    }

    return proc->pgid;
}

int sys_setpgid(pid_t pid, pid_t pgrp) {
    struct process *proc = thread_self->proc;

    if (pid == 0 && pgrp == 0) {
        proc->pgid = proc->pid;
        return 0;
    }

    if (pid == proc->pid) {
        proc->pgid = pgrp;
        return 0;
    }
    // Find child with pid pid (guess only children can be setpgid'd)
    struct process *chld = process_child(proc, pid);
    if (!chld) {
        return -ESRCH;
    }
    if (chld->pgid != proc->pgid) {
        return -EACCES;
    }
    chld->pgid = pgrp;

    return 0;
}

int sys_setuid(uid_t uid) {
    struct process *proc = thread_self->proc;
    _assert(proc);

    if (proc->ioctx.uid != 0) {
        return -EACCES;
    }

    proc->ioctx.uid = uid;
    return 0;
}

int sys_setgid(gid_t gid) {
    struct process *proc = thread_self->proc;
    _assert(proc);

    if (proc->ioctx.gid != 0 && proc->ioctx.uid != 0) {
        return -EACCES;
    }

    proc->ioctx.gid = gid;
    return 0;
}

uid_t sys_getuid(void) {
    return thread_self->proc->ioctx.uid;
}

gid_t sys_getgid(void) {
    return thread_self->proc->ioctx.gid;
}

pid_t sys_setsid(void) {
    struct process *proc = thread_self->proc;
    _assert(proc);
    if (proc->ctty) {
        // Close current tty filedes
        for (size_t i = 0; i < THREAD_MAX_FDS; ++i) {
            if (proc->fds[i] && !ofile_is_socket(proc->fds[i])) {
                if (proc->fds[i]->file.vnode == proc->ctty) {
                    kdebug("setsid: detaching fd %d from #%d (%s)\n", i, proc->pid, proc->name);
                    ofile_close(&proc->ioctx, proc->fds[i]);
                    proc->fds[i] = NULL;
                }
            }
        }
        proc->ctty = NULL;
    } else {
        kdebug("setsid: #%d (%s) didn't have a tty\n", proc->pid, proc->name);
    }

    return proc->pid;
}
