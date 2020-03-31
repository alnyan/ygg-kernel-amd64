#include "user/errno.h"
#include "arch/amd64/hw/timer.h"
#include "arch/amd64/cpu.h"
#include "fs/ofile.h"
#include "sys/char/ring.h"
#include "sys/char/chr.h"
#include "sys/sys_file.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "net/socket.h"
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

    if (of->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
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

    if (of->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
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

    if (thr->fds[fd]->flags & OF_SOCKET) {
        net_close(&thr->ioctx, thr->fds[fd]);
        kfree(thr->fds[fd]);
    } else {
        vfs_close(&thr->ioctx, thr->fds[fd]);
        _assert(thr->fds[fd]->file.refcount >= 0);
        if (!thr->fds[fd]->file.refcount) {
            kfree(thr->fds[fd]);
        }
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

    if (ofile->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
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

    if (of->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
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

    if (thr->fds[fd]->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
    }

    return vfs_readdir(&thr->ioctx, thr->fds[fd], ent);
}

static int sys_select_get_ready(struct ofile *fd) {
    if (fd->flags & OF_SOCKET) {
        return socket_has_data(&fd->socket);
    } else {
        struct vnode *vn = fd->file.vnode;
        _assert(vn);

        switch (vn->type) {
        case VN_CHR: {
                struct chrdev *chr = vn->dev;
                _assert(chr);
                if (chr->type == CHRDEV_TTY && (chr->tc.c_iflag & ICANON)) {
                    return (chr->buffer.flags & (RING_SIGNAL_RET | RING_SIGNAL_EOF | RING_SIGNAL_BRK));
                } else {
                    return !!ring_readable(&chr->buffer);
                }
            }
        default:
            panic("Not implemented\n");
        }
    }
}

static struct io_notify *sys_select_get_wait(struct ofile *fd) {
    if (fd->flags & OF_SOCKET) {
        return &fd->socket.rx_notify;
    } else {
        struct vnode *vn = fd->file.vnode;
        _assert(vn);

        switch (vn->type) {
        case VN_CHR: {
                struct chrdev *chr = vn->dev;
                _assert(chr);
                return &chr->buffer.wait;
            }
        default:
            panic("Not implemented\n");
        }
    }
}

int sys_select(int n, fd_set *inp, fd_set *outp, fd_set *excp, struct timeval *tv) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    // Not yet implemented
    _assert(!outp);
    _assert(!excp);

    if (!inp) {
        return 0;
    }

    fd_set _inp;
    memcpy(&_inp, inp, sizeof(fd_set));
    FD_ZERO(inp);

    // Check fds
    for (int i = 0; i < n; ++i) {
        if (FD_ISSET(i, &_inp)) {
            struct ofile *fd = thr->fds[i];

            if (!sys_select_get_wait(fd)) {
                // Can't wait on that fd
                return -EBADF;
            }
        }
    }

    uint64_t deadline = (uint64_t) -1;
    if (tv) {
        deadline = tv->tv_sec * 1000000000ULL + tv->tv_usec * 1000ULL + system_time;
    }
    int res;

    list_head_init(&thr->wait_head);
    for (int i = 0; i < n; ++i) {
        if (FD_ISSET(i, &_inp)) {
            struct ofile *fd = thr->fds[i];
            _assert(fd);

            struct io_notify *w = sys_select_get_wait(fd);
            _assert(w);

            thread_wait_io_add(thr, w);
        }
    }

    thr->sleep_deadline = deadline;
    thread_wait_io_add(thr, &thr->sleep_notify);
    timer_add_sleep(thr);

    struct io_notify *result;
    int ready = 0;

    while (1) {
        res = 0;
        // Check if data is available in any of the FDs
        for (int i = 0; i < n; ++i) {
            if (FD_ISSET(i, &_inp)) {
                struct ofile *fd = thr->fds[i];
                _assert(fd);

                if (sys_select_get_ready(fd)) {
                    // Data available, don't wait
                    FD_SET(i, inp);
                    res = 1;
                    timer_remove_sleep(thr);
                    break;
                }
            }
        }

        if (res) {
            break;
        }

        // Perform a wait for any single event
        res = thread_wait_io_any(thr, &result);
        ready = 1;

        if (res < 0) {
            // Likely interrupted
            //timer_remove_sleep(thr);
            break;
        }

        // Check if request timed out
        if (result == &thr->sleep_notify) {
            res = 0;
            break;
        }
    }

    // Remove select()ed io_notify structures from wait list
    thread_wait_io_clear(thr);

    return res;
}
