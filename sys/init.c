#include "sys/user/fcntl.h"
#include "sys/user/errno.h"
#include "sys/amd64/cpu.h"
#include "sys/sys_proc.h"
#include "sys/fs/ofile.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/fs/vfs.h"
#include "sys/debug.h"
#include "sys/sched.h"
#include "sys/heap.h"
#include "sys/dev.h"

struct thread user_init = {0};

static void user_init_func(void *arg) {
    kdebug("Starting user init\n");

    struct vfs_ioctx *ioctx = &thread_self->ioctx;
    struct vnode *root_dev, *tty_dev;
    int res;
    // Mount root
    if ((res = dev_find(DEV_CLASS_BLOCK, "ram0", &root_dev)) != 0) {
        kerror("ram0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    if ((res = vfs_mount(ioctx, "/", root_dev->dev, "ustar", NULL)) != 0) {
        kerror("mount: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    // Open STDOUT_FILENO and STDERR_FILENO
    struct ofile *fd_stdout = kmalloc(sizeof(struct ofile));
    struct ofile *fd_stdin = kmalloc(sizeof(struct ofile));

    if ((res = dev_find(DEV_CLASS_CHAR, "tty0", &tty_dev)) != 0) {
        kerror("tty0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    if ((res = vfs_open_vnode(ioctx, fd_stdin, tty_dev, O_RDONLY)) != 0) {
        kerror("tty0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    if ((res = vfs_open_vnode(ioctx, fd_stdout, tty_dev, O_WRONLY)) != 0) {
        kerror("tty0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    thread_self->fds[0] = fd_stdin;
    thread_self->fds[1] = fd_stdout;
    thread_self->fds[2] = fd_stdout;
    // Duplicate the FD
    ++fd_stdout->refcount;

    _assert(fd_stdin->refcount == 1);
    _assert(fd_stdout->refcount == 2);

    sys_execve("/test", NULL, NULL);

    while (1) {
        asm volatile ("hlt");
    }
}

void user_init_start(void) {
    thread_init(&user_init, (uintptr_t) user_init_func, NULL, 0);
    user_init.pid = thread_alloc_pid(0);
    sched_queue(&user_init);
}
