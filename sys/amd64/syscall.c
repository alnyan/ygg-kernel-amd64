#include "sys/amd64/cpu.h"
#include "sys/syscall.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/types.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/fs/vfs.h"

static ssize_t sys_read(int fd, void *buf, size_t lim);
static ssize_t sys_write(int fd, const void *buf, size_t lim);
static int sys_open(const char *filename, int flags, int mode);
static void sys_close(int fd);
static int sys_stat(const char *filename, struct stat *st);
static void sys_exit(int status);

__attribute__((noreturn)) void amd64_syscall_yield_stopped(void);

intptr_t amd64_syscall(uintptr_t rdi, uintptr_t rsi, uintptr_t rdx, uintptr_t rcx, uintptr_t r10, uintptr_t rax) {
    switch (rax) {
    case SYSCALL_NR_READ:
        return sys_read((int) rdi, (void *) rsi, (size_t) rdx);
    case SYSCALL_NR_WRITE:
        return sys_write((int) rdi, (const void *) rsi, (size_t) rdx);
    case SYSCALL_NR_OPEN:
        return sys_open((const char *) rdi, (int) rsi, (int) rdx);
    case SYSCALL_NR_CLOSE:
        sys_close((int) rdi);
        return 0;
    case SYSCALL_NR_STAT:
        return sys_stat((const char *) rdi, (struct stat *) rsi);

    case SYSCALL_NR_EXIT:
        sys_exit((int) rdi);
        amd64_syscall_yield_stopped();

    default:
        kdebug("unknown syscall: %u\n", rax);
        return -1;
    }
    return 0;
}

static ssize_t sys_read(int fd, void *buf, size_t lim) {
    if (fd >= 4 || fd < 0) {
        return -EBADF;
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    struct ofile *of = &thr->fds[fd];

    if (!of->vnode) {
        return -EBADF;
    }

    return vfs_read(&thr->ioctx, of, buf, lim);
}

static ssize_t sys_write(int fd, const void *buf, size_t lim) {
    if (fd >= 4 || fd < 0) {
        return -EBADF;
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    struct ofile *of = &thr->fds[fd];

    if (!of->vnode) {
        return -EBADF;
    }

    return vfs_write(&thr->ioctx, of, buf, lim);
}

static int sys_open(const char *filename, int flags, int mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    int fd = -1;

    for (int i = 0; i < 4; ++i) {
        if (!thr->fds[i].vnode) {
            fd = i;
            break;
        }
    }

    if (fd != -1) {
        struct ofile *of = &thr->fds[fd];

        int res = vfs_open(&thr->ioctx, of, filename, mode, flags);

        if (res != 0) {
            of->vnode = NULL;
            return res;
        }

        return fd;
    } else {
        return -1;
    }
}

static void sys_close(int fd) {
    if (fd >= 4 || fd < 0) {
        return;
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    struct ofile *of = &thr->fds[fd];

    if (!of->vnode) {
        return;
    }

    vfs_close(&thr->ioctx, of);
    of->vnode = NULL;
}

static int sys_stat(const char *filename, struct stat *st) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    int res = vfs_stat(&thr->ioctx, filename, st);

    return res;
}

static void sys_exit(int status) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    kdebug("%u exited with code %d\n", thr->pid, status);
    thr->flags |= THREAD_STOPPED;
}

