#include <config.h>
#include "user/errno.h"
#include "arch/amd64/hw/timer.h"
#include "arch/amd64/cpu.h"
#include "fs/ofile.h"
#include "user/fcntl.h"
#include "sys/char/ring.h"
#include "sys/char/pipe.h"
#include "sys/char/chr.h"
#include "sys/sys_file.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "net/socket.h"
#include "sys/debug.h"
#include "sys/heap.h"

static inline struct ofile *get_fd(int fd) {
    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return NULL;
    }
    return thread_self->proc->fds[fd];
}

static inline struct vfs_ioctx *get_ioctx(void) {
    return &thread_self->proc->ioctx;
}

static inline int get_at_vnode(int dfd, struct vnode **at, int flags) {
    if (dfd == AT_FDCWD) {
        *at = get_ioctx()->cwd_vnode;
        return 0;
    } else {
        struct ofile *fd = get_fd(dfd);
        if (!fd) {
            return -EBADF;
        }
        if (!(flags & AT_EMPTY_PATH) && !(fd->flags & OF_DIRECTORY)) {
            return -ENOTDIR;
        }
        *at = fd->file.vnode;
        return 0;
    }
}

ssize_t sys_read(int fd, void *data, size_t lim) {
    userptr_check(data);
    struct ofile *of;

    if ((of = get_fd(fd)) == NULL) {
        return -EBADF;
    }

    if (of->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
    }

    return vfs_read(get_ioctx(), of, data, lim);
}

ssize_t sys_write(int fd, const void *data, size_t lim) {
    userptr_check(data);
    struct ofile *of;

    if ((of = get_fd(fd)) == NULL) {
        return -EBADF;
    }

    if (of->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
    }

    return vfs_write(get_ioctx(), of, data, lim);
}

int sys_creat(const char *pathname, int mode) {
    userptr_check(pathname);
    return -EINVAL;
}

int sys_mkdirat(int dfd, const char *pathname, int mode) {
    userptr_check(pathname);
    _assert(pathname);
    struct vnode *at;
    int res;

    if ((res = get_at_vnode(dfd, &at, 0)) != 0) {
        return res;
    }

    return vfs_mkdirat(get_ioctx(), at, pathname, mode);
}

int sys_unlinkat(int dfd, const char *pathname, int flags) {
    userptr_check(pathname);
    struct vnode *at;
    int res;

    if ((res = get_at_vnode(dfd, &at, 0))) {
        return res;
    }

    return vfs_unlinkat(get_ioctx(), at, pathname, flags);
}

int sys_truncate(const char *pathname, off_t length) {
    userptr_check(pathname);
    struct vnode *node;
    struct vfs_ioctx *ioctx = get_ioctx();
    int res;

    if ((res = vfs_find(ioctx, ioctx->cwd_vnode, pathname, 0, &node)) != 0) {
        return res;
    }

    return vfs_ftruncate(ioctx, node, length);
}

int sys_ftruncate(int fd, off_t length) {
    struct ofile *of = get_fd(fd);
    if (!of) {
        return -EBADF;
    }
    if (of->flags & OF_SOCKET) {
        kwarn("truncate() on socket\n");
        return -EINVAL;
    }
    return vfs_ftruncate(get_ioctx(), of->file.vnode, length);
}

int sys_chdir(const char *filename) {
    userptr_check(filename);
    return vfs_setcwd(get_ioctx(), filename);
}

// Kinda incompatible with linux, but who cares as long as it's
// POSIX on the libc side
int sys_getcwd(char *buf, size_t lim) {
    userptr_check(buf);
    struct vfs_ioctx *ioctx = get_ioctx();

    if (!ioctx->cwd_vnode) {
        if (lim < 2) {
            return -1;
        }

        buf[0] = '/';
        buf[1] = 0;

        return 0;
    } else {
        char tmpbuf[PATH_MAX];
        vfs_vnode_path(tmpbuf, ioctx->cwd_vnode);
        if (lim <= strlen(tmpbuf)) {
            return -1;
        }

        strcpy(buf, tmpbuf);

        return 0;
    }
}

int sys_openat(int dfd, const char *filename, int flags, int mode) {
    userptr_check(filename);
    struct process *proc = thread_self->proc;
    struct vnode *at;
    int fd = -1;
    int res;

    if ((res = get_at_vnode(dfd, &at, 0)) != 0) {
        return res;
    }

    // XXX: This should be atomic
    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        return -EMFILE;
    }

    struct ofile *ofile = ofile_create();

    if ((res = vfs_openat(&proc->ioctx, ofile, at, filename, flags, mode)) != 0) {
        ofile_destroy(ofile);
        return res;
    }

    proc->fds[fd] = ofile_dup(ofile);
    _assert(proc->fds[fd]->refcount == 1);

    return fd;
}

void sys_close(int fd) {
    struct process *proc = thread_self->proc;

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return;
    }

    if (proc->fds[fd] == NULL) {
        return;
    }

    ofile_close(&proc->ioctx, proc->fds[fd]);
    proc->fds[fd] = NULL;
}

int sys_fstatat(int dfd, const char *pathname, struct stat *st, int flags) {
    if (!(flags & AT_EMPTY_PATH)) {
        userptr_check(pathname);
    }
    userptr_check(st);

    struct vnode *at;
    int res;

    if ((res = get_at_vnode(dfd, &at, flags)) != 0) {
        return res;
    }

    return vfs_fstatat(get_ioctx(), at, pathname, st, flags);
}

int sys_faccessat(int dfd, const char *pathname, int mode, int flags) {
    userptr_check(pathname);

    struct vnode *at;
    int res;

    if ((res = get_at_vnode(dfd, &at, flags)) != 0) {
        return res;
    }

    return vfs_faccessat(get_ioctx(), at, pathname, mode, flags);
}

int sys_pipe(int *filedes) {
    userptr_check(filedes);
    struct process *proc = thread_self->proc;
    struct ofile *read_end, *write_end;
    int fd0 = -1, fd1 = -1;
    int res;

    if ((res = pipe_create(&read_end, &write_end)) != 0) {
        return res;
    }


    // XXX: This should be atomic
    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (!proc->fds[i]) {
            if (fd0 == -1) {
                fd0 = i;
            } else {
                fd1 = i;
                break;
            }
        }
    }
    if (fd0 == -1 || fd1 == -1) {
        return -EMFILE;
    }

    proc->fds[fd0] = ofile_dup(read_end);
    proc->fds[fd1] = ofile_dup(write_end);

    filedes[0] = fd0;
    filedes[1] = fd1;

    return 0;
}

int sys_dup(int from) {
    struct process *proc = thread_self->proc;

    int fd = -1;
    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }

    return sys_dup2(from, fd);
}

int sys_dup2(int from, int to) {
    if (from < 0 || to < 0 || from >= THREAD_MAX_FDS || to >= THREAD_MAX_FDS) {
        return -EBADF;
    }
    if (to == from) {
        return to;
    }

    // TODO: process_self macro?
    struct process *proc = thread_self->proc;

    if (!proc->fds[from]) {
        return -EBADF;
    }

    sys_close(to);
    _assert(!proc->fds[to]);

    proc->fds[to] = ofile_dup(proc->fds[from]);

    return to;
}

int sys_openpty(int *master, int *slave) {
    return -EINVAL;
}

int sys_chmod(const char *path, mode_t mode) {
    userptr_check(path);
    _assert(path);

    return vfs_chmod(get_ioctx(), path, mode);
}

int sys_chown(const char *path, uid_t uid, gid_t gid) {
    userptr_check(path);
    _assert(path);

    return vfs_chown(get_ioctx(), path, uid, gid);
}

off_t sys_lseek(int fd, off_t offset, int whence) {
    struct ofile *ofile;

    if ((ofile = get_fd(fd)) == NULL) {
        return -EBADF;
    }

    if (ofile->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
    }

    return vfs_lseek(get_ioctx(), ofile, offset, whence);
}

int sys_ioctl(int fd, unsigned int cmd, void *arg) {
    struct ofile *of;
    if (!(of = get_fd(fd))) {
        return -EBADF;
    }

    if (of->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
    }

    return vfs_ioctl(get_ioctx(), of, cmd, arg);
}

ssize_t sys_readdir(int fd, struct dirent *ent) {
    userptr_check(ent);
    struct ofile *of;

    if ((of = get_fd(fd)) == NULL) {
        return -EBADF;
    }

    if (of->flags & OF_SOCKET) {
        kwarn("Invalid operation on socket\n");
        return -EINVAL;
    }

    return vfs_readdir(get_ioctx(), of, ent);
}

int sys_mknod(const char *filename, int mode, unsigned int dev) {
    userptr_check(filename);

    int type = mode & S_IFMT;
    int res;
    struct vnode *node;

    if ((res = vfs_mknod(get_ioctx(), filename, mode, &node)) != 0) {
        return res;
    }

    _assert(node);

    switch (type) {
    case S_IFIFO:
        return pipe_fifo_create(node);
    default:
        return -EINVAL;
    }
}

static int sys_select_get_ready(struct ofile *fd) {
#if defined(ENABLE_NET)
    if (fd->flags & OF_SOCKET) {
        return socket_has_data(&fd->socket);
    } else
#endif
    {
        struct vnode *vn = fd->file.vnode;
        _assert(vn);

        switch (vn->type) {
        case VN_CHR: {
                struct chrdev *chr = vn->dev;
                _assert(chr);
                int r;
                if (chr->type == CHRDEV_TTY && (chr->tc.c_lflag & ICANON)) {
                    r = (chr->buffer.flags & (RING_SIGNAL_RET | RING_SIGNAL_EOF | RING_SIGNAL_BRK));
                } else {
                    r = !!ring_readable(&chr->buffer);
                }
                return r;
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
    if (inp) {
        userptr_check(inp);
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    struct process *proc = thr->proc;
    _assert(proc);

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
            struct ofile *fd = proc->fds[i];

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
            struct ofile *fd = proc->fds[i];
            _assert(fd);

            struct io_notify *w = sys_select_get_wait(fd);
            _assert(w);

            thread_wait_io_add(thr, w);
        }
    }

    struct io_notify *result;
    int ready = 0;

    while (1) {
        res = 0;
        // Check if data is available in any of the FDs
        thread_wait_io_clear(thr);
        for (int i = 0; i < n; ++i) {
            if (FD_ISSET(i, &_inp)) {
                struct ofile *fd = proc->fds[i];
                _assert(fd);

                if (sys_select_get_ready(fd)) {
                    // Data available, don't wait
                    FD_SET(i, inp);
                    res = 1;
                    timer_remove_sleep(thr);
                    break;
                }
                struct io_notify *w = sys_select_get_wait(fd);
                _assert(w);

                thread_wait_io_add(thr, w);
            }
        }

        if (res) {
            break;
        }

        if (deadline != (uint64_t) -1) {
            thr->sleep_deadline = deadline;
            thread_wait_io_add(thr, &thr->sleep_notify);
            timer_add_sleep(thr);
        }

        // Perform a wait for any single event
        res = thread_wait_io_any(thr, &result);
        ready = 1;
        timer_remove_sleep(thr);

        if (res < 0) {
            // Likely interrupted
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
