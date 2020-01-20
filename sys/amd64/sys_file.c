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

int sys_select(int nfds, fd_set *rd, fd_set *wr, fd_set *exc, struct timeval *tv) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    // Not implemented yet
    _assert(!wr);
    _assert(!exc);

    // TODO: write fd_set
    fd_set _rd;
    memcpy(&_rd, rd, sizeof(fd_set));
    FD_ZERO(rd);

    if (!rd) {
        return 0;
    }

    // Check file descriptors
    for (int i = 0; i < nfds; ++i) {
        if (FD_ISSET(i, &_rd)) {
            if (i > THREAD_MAX_FDS) {
                kwarn("Bad FD: %d\n", i);
                return -EBADF;
            }
            struct ofile *fd = thr->fds[i];
            if (!fd) {
                kwarn("Bad FD: %d\n", i);
                return -EBADF;
            }

            _assert(fd->vnode);
            if (fd->vnode->type != VN_CHR) {
                // select() does not make sense for non-char devices yet
                FD_SET(i, rd);
                return 0;
            }
        }
    }

    uint64_t deadline = tv->tv_sec * 1000000000ULL + tv->tv_usec * 1000ULL + system_time;
    int res;

    while (1) {
        // Check for any ready FD
        for (int i = 0; i < nfds; ++i) {
            if (FD_ISSET(i, &_rd)) {
                struct ofile *fd = thr->fds[i];
                _assert(fd && fd->vnode && fd->vnode->type == VN_CHR);

                struct chrdev *chr = fd->vnode->dev;
                _assert(chr);

                if (ring_avail(&chr->buffer) > 0) {
                    FD_SET(i, rd);
                    res = 0;
                    goto done;
                }
            }
        }

        // Check timer deadline
        if (system_time >= deadline) {
            res = 0;
            goto done;
        }

        // Yield
        asm volatile ("sti; hlt; cli");
    }

done:
    return res;
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

int sys_isatty(int fd) {
    struct thread *thr = get_cpu()->thread;
    struct ofile *of;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if (!(of = thr->fds[fd])) {
        return -EBADF;
    }

    _assert(of->vnode);
    if (of->vnode->type == VN_CHR) {
        struct chrdev *chr = of->vnode->dev;
        _assert(chr);

        // TODO: somehow check that this is a tty
        //       via flags
        return 1;
    }

    return -ENOTTY;
}
