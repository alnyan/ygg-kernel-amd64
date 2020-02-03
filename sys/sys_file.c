#include "sys/user/errno.h"
#include "sys/amd64/cpu.h"
#include "sys/fs/ofile.h"
#include "sys/sys_file.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"

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

int sys_creat(const char *pathname, int mode) {
    return -EINVAL;
}

int sys_mkdir(const char *pathname, int mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(pathname);
    return vfs_mkdir(&thr->ioctx, pathname, mode);
}

int sys_unlink(const char *pathname) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(pathname);
    return vfs_unlink(&thr->ioctx, pathname);
}

int sys_rmdir(const char *pathname) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(pathname);
    return vfs_rmdir(&thr->ioctx, pathname);
}

int sys_chdir(const char *filename) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    return vfs_setcwd(&thr->ioctx, filename);
}

// Kinda incompatible with linux, but who cares as long as it's
// POSIX on the libc side
int sys_getcwd(char *buf, size_t lim) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    if (!thr->ioctx.cwd_vnode) {
        if (lim < 2) {
            return -1;
        }

        buf[0] = '/';
        buf[1] = 0;

        return 0;
    } else {
        char tmpbuf[PATH_MAX];
        vfs_vnode_path(tmpbuf, thr->ioctx.cwd_vnode);
        if (lim <= strlen(tmpbuf)) {
            return -1;
        }

        strcpy(buf, tmpbuf);

        return 0;
    }
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
    _assert(thr->fds[fd]->refcount >= 0);
    if (!thr->fds[fd]->refcount) {
        kfree(thr->fds[fd]);
    }
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

int sys_chmod(const char *path, mode_t mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(path);

    return vfs_chmod(&thr->ioctx, path, mode);
}

int sys_chown(const char *path, uid_t uid, gid_t gid) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(path);

    return vfs_chown(&thr->ioctx, path, uid, gid);
}

off_t sys_lseek(int fd, off_t offset, int whence) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    struct ofile *ofile;

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((ofile = thr->fds[fd]) == NULL) {
        return -EBADF;
    }

    return vfs_lseek(&thr->ioctx, ofile, offset, whence);
}

int sys_ioctl(int fd, unsigned int cmd, void *arg) {
    struct thread *thr = get_cpu()->thread;
    struct ofile *of;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if (!(of = thr->fds[fd])) {
        return -EBADF;
    }

    return vfs_ioctl(&thr->ioctx, of, cmd, arg);
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

