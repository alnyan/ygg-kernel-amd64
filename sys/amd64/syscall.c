#include "sys/amd64/cpu.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/types.h"
#include "sys/debug.h"
#include "sys/errno.h"

static ssize_t sys_read(int fd, void *buf, size_t lim);
static ssize_t sys_write(int fd, const void *buf, size_t lim);

uint8_t irq1_key = 0;

intptr_t amd64_syscall(uintptr_t rdi, uintptr_t rsi, uintptr_t rdx, uintptr_t rcx, uintptr_t r10, uintptr_t rax) {
    switch (rax) {
    case 0:
        return sys_read((int) rdi, (void *) rsi, (size_t) rdx);
    case 1:
        return sys_write((int) rdi, (const void *) rsi, (size_t) rdx);
    default:
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
