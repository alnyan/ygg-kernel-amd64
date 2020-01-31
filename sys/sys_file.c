#include "sys/user/errno.h"
#include "sys/amd64/cpu.h"
#include "sys/sys_file.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/debug.h"

ssize_t sys_read(int fd, void *data, size_t lim) {
    struct thread *thr = thread_self;
    struct ofile *of;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = thr->fds[fd]) == NULL) {
        return -EBADF;
    }

    return vfs_read(&thr->ioctx, of, data, lim);
}

ssize_t sys_write(int fd, const void *data, size_t lim) {
    struct thread *thr = thread_self;
    struct ofile *of;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = thr->fds[fd]) == NULL) {
        return -EBADF;
    }

    return vfs_write(&thr->ioctx, of, data, lim);
}
