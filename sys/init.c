#include "sys/amd64/mm/pool.h"
#include "sys/amd64/syscall.h"
#include "sys/binfmt_elf.h"
#include "sys/amd64/cpu.h"
#include "sys/fs/ext2.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/sched.h"
#include "sys/errno.h"
#include "sys/fcntl.h"
#include "sys/panic.h"
#include "sys/heap.h"
#include "sys/dev.h"
#include "sys/mm.h"

static void user_init_start(void) {
    // TODO: Instead of allocating and reading the whole buffer, rewrite binfmt_elf in a way
    //       to use lseek and stuff
    const char *init_filename = "/init";
    struct ofile fd;
    struct stat st;
    struct thread *user_init;
    char *file_buffer = NULL;
    mm_space_t space;
    ssize_t bread;
    int res;

    if ((res = vfs_stat(kernel_ioctx, init_filename, &st)) != 0) {
        panic("%s: %s\n", init_filename, kstrerror(res));
    }

    if ((res = vfs_open(kernel_ioctx, &fd, init_filename, O_RDONLY | O_EXEC, 0)) != 0) {
        panic("%s: %s\n", init_filename, kstrerror(res));
    }

    // TODO: use thread_create or something
    user_init = kmalloc(sizeof(struct thread));
    _assert(user_init);

    space = amd64_mm_pool_alloc();
    _assert(space);

    mm_space_clone(space, mm_kernel, MM_CLONE_FLG_KERNEL);

    if ((res = thread_init(user_init,
                           space,
                           0,
                           0,
                           0,
                           0,
                           0,
                           0,
                           THREAD_INIT_CTX,
                           NULL)) != 0) {
        panic("Failed to initialize user init process: %s\n", kstrerror(res));
    }

    //file_buffer = kmalloc(st.st_size);
    //_assert(file_buffer);

    //if ((bread = vfs_read(kernel_ioctx, &fd, file_buffer, st.st_size)) != st.st_size) {
    //    panic("Failed to read init binary\n");
    //}

    if ((res = elf_load(user_init, kernel_ioctx, &fd)) != 0) {
        panic("Failed to load ELF binary: %s\n", kstrerror(res));
    }

    vfs_close(kernel_ioctx, &fd);

    //kfree(file_buffer);

    // Setup I/O context for init task
    struct ofile *stdin, *stdout, *stderr;

    stdin = kmalloc(sizeof(struct ofile));
    _assert(stdin);
    stdout = kmalloc(sizeof(struct ofile));
    _assert(stdout);
    stderr = stdout;

    // TODO: use dev_find and open its node instead
    struct vnode *stdout_device;
    if ((res = dev_find(DEV_CLASS_CHAR, "tty0", &stdout_device)) != 0) {
        panic("%s: %s\n", "tty0", kstrerror(res));
    }

    if ((res = vfs_open_vnode(&user_init->ioctx, stdin, stdout_device, O_RDONLY)) != 0) {
        panic("Failed to open stdin for init task: %s\n", kstrerror(res));
    }
    if ((res = vfs_open_vnode(&user_init->ioctx, stdout, stdout_device, O_WRONLY)) != 0) {
        panic("Failed to open stdout/stderr for init task: %s\n", kstrerror(res));
    }
    ++stdout->refcount;

    // TODO: use STDIN_/STDOUT_/STDERR_FILENO macros
    user_init->fds[0] = stdin;
    user_init->fds[1] = stdout;
    user_init->fds[2] = stderr;

    user_init->ioctx.uid = 0;
    user_init->ioctx.gid = 0;
    user_init->ioctx.cwd_vnode = NULL;

    sched_add(user_init);
}

void init_func(void *arg) {
    const char *root_dev_name = "ram0";
    struct vnode *root_dev;
    int res;

    // Be [init] now
    thread_set_name(get_cpu()->thread, "init");

    // Create an alias for root device
    if ((res = dev_find(DEV_CLASS_BLOCK, root_dev_name, &root_dev)) != 0) {
        panic("Failed to find root device: %s\n", root_dev_name);
    }
    dev_add_link("root", root_dev);

    ext2_class_init();

    // Mount rootfs
    if ((res = vfs_mount(kernel_ioctx, "/", root_dev->dev, "ustar", NULL)) != 0) {
        panic("Failed to mount root device: %s\n", kstrerror(res));
    }

    //// Mount something else
    //if ((res = dev_find(DEV_CLASS_BLOCK, "sda1", &root_dev)) != 0) {
    //    panic("sda1: %s\n", kstrerror(res));
    //}
    //if ((res = vfs_mount(kernel_ioctx, "/mnt", root_dev->dev, "ext2", NULL)) != 0) {
    //    panic("Failed to mount root device: %s\n", kstrerror(res));
    //}

    // Start user init binary
    user_init_start();

    struct thread *this = get_cpu()->thread;
    this->exit_code = 0;
    this->flags |= THREAD_STOPPED;
    amd64_syscall_yield_stopped();
}
