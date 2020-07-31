#include "user/fcntl.h"
#include "user/errno.h"
#include "arch/amd64/cpu.h"
#include "sys/sys_proc.h"
#include "fs/ofile.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "fs/vfs.h"
#include "sys/debug.h"
#include "sys/sched.h"
#include "sys/heap.h"
#include "sys/dev.h"

struct process user_init = {0};
//struct thread user_init = {0};

static void user_init_func(void *arg) {
    kdebug("Starting user init\n");

    struct vfs_ioctx *ioctx = &thread_self->proc->ioctx;
    struct vnode *root_dev, *tty_dev;
    int res;
    // Mount root
    if ((res = dev_find(DEV_CLASS_BLOCK, "ram0", &root_dev)) != 0) {
        kerror("ram0: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    if ((res = vfs_mount(ioctx, "/", root_dev->dev, "ramfs", 0, NULL)) != 0) {
        kerror("mount: %s\n", kstrerror(res));
        panic("Fail\n");
    }

    // Open STDOUT_FILENO and STDERR_FILENO
    struct ofile *fd_stdout = ofile_create();
    struct ofile *fd_stdin = ofile_create();

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

    thread_self->proc->fds[0] = ofile_dup(fd_stdin);
    thread_self->proc->fds[1] = ofile_dup(fd_stdout);
    thread_self->proc->fds[2] = ofile_dup(fd_stdout);

    _assert(fd_stdin->refcount == 1);
    _assert(fd_stdout->refcount == 2);

    thread_self->proc->ctty = tty_dev;

    const char *argp[] = {
        "/init", NULL
    };
    // &argp[1] - just an empty envp
    sys_execve(argp[0], argp, &argp[1]);

    while (1) {
        asm volatile ("hlt");
    }
}

void user_init_start(void) {
    process_init_thread(&user_init, (uintptr_t) user_init_func, NULL, 0);
    sched_queue(process_first_thread(&user_init));
}
