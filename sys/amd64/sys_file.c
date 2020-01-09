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

ssize_t sys_read(int fd, void *buf, size_t lim) {
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

ssize_t sys_write(int fd, const void *buf, size_t lim) {
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

ssize_t sys_readdir(int fd, struct dirent *ent) {
    if (fd >= 4 || fd < 0) {
        return -EBADF;
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    struct ofile *of = &thr->fds[fd];

    if (!of->vnode) {
        return -EBADF;
    }

    struct dirent *src = vfs_readdir(&thr->ioctx, of);

    if (!src) {
        return -1;
    }

    // XXX: safe?
    memcpy(ent, src, sizeof(struct dirent) + strlen(src->d_name));

    return src->d_reclen;
}

int sys_creat(const char *pathname, int mode) {
    return sys_open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int sys_mkdir(const char *pathname, int mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    return vfs_mkdir(&thr->ioctx, pathname, mode);
}

int sys_unlink(const char *pathname) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    return vfs_unlink(&thr->ioctx, pathname);
}

int sys_chdir(const char *filename) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(filename);

    return vfs_chdir(&thr->ioctx, filename);
}

// Kinda incompatible with linux, but who cares as long as it's
// POSIX on the libc side
int sys_getcwd(char *buf, size_t lim) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(buf);
    char tmpbuf[512];
    size_t len;

    vfs_vnode_path(tmpbuf, thr->ioctx.cwd_vnode);
    len = strlen(tmpbuf);

    if (lim < len + 1) {
        return -1;
    }

    strcpy(buf, tmpbuf);

    return 0;
}

int sys_open(const char *filename, int flags, int mode) {
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

void sys_close(int fd) {
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

int sys_stat(const char *filename, struct stat *st) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    int res = vfs_stat(&thr->ioctx, filename, st);

    return res;
}

int sys_access(const char *path, int mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return vfs_access(&thr->ioctx, path, mode);
}

int sys_openpty(int *master, int *slave) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    // Find two free file descriptors
    int fd_master = -1, fd_slave = -1;
    int res;

    for (int i = 0; i < 4; ++i) {
        if (!thr->fds[i].vnode) {
            if (fd_master == -1) {
                fd_master = i;
                continue;
            }
            fd_slave = i;
            break;
        }
    }

    if (fd_master == -1 || fd_slave == -1) {
        return -EMFILE;
    }

    struct ofile *of_master = &thr->fds[fd_master];
    struct ofile *of_slave = &thr->fds[fd_slave];

    struct pty *pty = pty_create();
    _assert(pty);

    of_master->vnode = pty->master;
    of_master->flags = O_RDWR;
    of_master->pos = 0;

    of_slave->vnode = pty->slave;
    of_slave->flags = O_RDWR;
    of_slave->pos = 0;

    *master = fd_master;
    *slave = fd_slave;

    return 0;
}

