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
    return -EINVAL;
    //if (fd >= 4 || fd < 0) {
    //    return -EBADF;
    //}
    //struct thread *thr = get_cpu()->thread;
    //_assert(thr);

    //struct ofile *of = &thr->fds[fd];

    //if (!of->vnode) {
    //    return -EBADF;
    //}

    //return vfs_read(&thr->ioctx, of, buf, lim);
}

ssize_t sys_write(int fd, const void *buf, size_t lim) {
    return -EINVAL;
}

ssize_t sys_readdir(int fd, struct dirent *ent) {
    return -EINVAL;
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
    return -EINVAL;
}

void sys_close(int fd) {
}

int sys_stat(const char *filename, struct stat *st) {
    return -EINVAL;
}

int sys_access(const char *path, int mode) {
    return -EINVAL;
}

int sys_openpty(int *master, int *slave) {
    return -EINVAL;
}

