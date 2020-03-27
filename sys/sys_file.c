#include "user/errno.h"
#include "arch/amd64/cpu.h"
#include "fs/ofile.h"
#include "sys/char/ring.h"
#include "sys/char/chr.h"
#include "sys/sys_file.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "net/net.h"

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

            if (!fd || (fd->flags & OF_SOCKET)) {
                return -EBADF;
            }

            _assert(fd->file.vnode);
            if (fd->file.vnode->type != VN_CHR) {
                kerror("Tried to select() on non-char device/file: %s\n", fd->file.vnode->name);
                return -ENOSYS;
            }
        }
    }

    uint64_t deadline = (uint64_t) -1;
    if (tv) {
        deadline = tv->tv_sec * 1000000000ULL + tv->tv_usec * 1000ULL + system_time;
    }
    int res;

    while (system_time < deadline) {
        for (int i = 0; i < n; ++i) {
            if (FD_ISSET(i, &_inp)) {
                struct ofile *fd = thr->fds[i];
                _assert(fd);
                struct vnode *vn = fd->file.vnode;
                _assert(vn && vn->type == VN_CHR);
                struct chrdev *chr = vn->dev;
                _assert(chr);

                if (chr->type == CHRDEV_TTY && (chr->tc.c_iflag & ICANON)) {
                    if (chr->buffer.flags & (RING_SIGNAL_RET | RING_SIGNAL_EOF | RING_SIGNAL_BRK)) {
                        FD_SET(i, inp);
                        return 1;
                    }
                } else {
                    if (ring_readable(&chr->buffer)) {
                        FD_SET(i, inp);
                        return 1;
                    }
                }
            }
        }

        // TODO: add something like "listen to all fds events"
        asm volatile ("sti; hlt");
        thread_check_signal(thr, 0);
    }

    return 0;
}
