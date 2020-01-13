#include "sys/amd64/sys_file.h"
#include "sys/amd64/cpu.h"
#include "sys/fs/vfs.h"
#include "sys/fs/pty.h"
#include "sys/thread.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/fcntl.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/heap.h"

ssize_t sys_read(int fd, void *buf, size_t lim) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if (thr->fds[fd] == NULL) {
        return -EBADF;
    }

    return vfs_read(&thr->ioctx, thr->fds[fd], buf, lim);
}

ssize_t sys_write(int fd, const void *buf, size_t lim) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if (thr->fds[fd] == NULL) {
        return -EBADF;
    }

    return vfs_write(&thr->ioctx, thr->fds[fd], buf, lim);
}

ssize_t sys_readdir(int fd, struct dirent *ent) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if (thr->fds[fd] == NULL) {
        return -EBADF;
    }

    return vfs_readdir(&thr->ioctx, thr->fds[fd], ent);
}

int sys_creat(const char *pathname, int mode) {
    return -EINVAL;
}

int sys_mkdir(const char *pathname, int mode) {
    return -EINVAL;
}

int sys_unlink(const char *pathname) {
    return -EINVAL;
}

int sys_rmdir(const char *pathname) {
    return -EINVAL;
}

int sys_chdir(const char *filename) {
    return -EINVAL;
}

// Kinda incompatible with linux, but who cares as long as it's
// POSIX on the libc side
int sys_getcwd(char *buf, size_t lim) {
    return -EINVAL;
}

int sys_open(const char *filename, int flags, int mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    int fd = -1;
    int res;

    // XXX: This should be atomic
    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (!thr->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        return -EMFILE;
    }

    struct ofile *ofile = kmalloc(sizeof(struct ofile));
    _assert(ofile);

    if ((res = vfs_open(&thr->ioctx, ofile, filename, flags, mode)) != 0) {
        kfree(ofile);
        return res;
    }

    thr->fds[fd] = ofile;
    return fd;
}

void sys_close(int fd) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return;
    }

    if (thr->fds[fd] == NULL) {
        return;
    }

    vfs_close(&thr->ioctx, thr->fds[fd]);
    thr->fds[fd] = NULL;
}

int sys_stat(const char *filename, struct stat *st) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(filename);
    _assert(st);

    return vfs_stat(&thr->ioctx, filename, st);
}

int sys_access(const char *path, int mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(path);

    return vfs_access(&thr->ioctx, path, mode);
}

int sys_openpty(int *master, int *slave) {
    return -EINVAL;
}

