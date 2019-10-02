#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/syscall.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/errno.h"

static ssize_t sys_write(int fd, const userspace void *buf, size_t lim);
static ssize_t sys_read(int fd, userspace void *buf, size_t lim);

uint64_t amd64_syscall(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t no) {
    switch (no) {
    case SYS_NR_READ:
        return sys_read(a0, (void *) a1, a2);
    case SYS_NR_WRITE:
        return sys_write(a0, (const void *) a1, a2);
    default:
        return -EINVAL;
    }
}

static ssize_t sys_write(int fd, const userspace void *buf, size_t lim) {
    struct thread_info *tinfo = thread_get(sched_current);
    _assert(tinfo);
    if (fd < 0 || (size_t) fd >= sizeof(tinfo->fds) / sizeof(tinfo->fds[0])) {
        return -EBADF;
    }
    struct ofile *of = &tinfo->fds[fd];
    if (of->vnode == NULL) {
        return -EBADF;
    }

    return vfs_write(&tinfo->ioctx, of, buf, lim);
}

static ssize_t sys_read(int fd, userspace void *buf, size_t lim) {
    struct thread_info *tinfo = thread_get(sched_current);
    _assert(tinfo);
    if (fd < 0 || (size_t) fd >= sizeof(tinfo->fds) / sizeof(tinfo->fds[0])) {
        return -EBADF;
    }
    struct ofile *of = &tinfo->fds[fd];
    if (of->vnode == NULL) {
        return -EBADF;
    }

    return vfs_read(&tinfo->ioctx, of, buf, lim);
}
