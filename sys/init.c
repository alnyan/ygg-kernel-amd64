#include "sys/amd64/mm/pool.h"
#include "sys/amd64/mm/map.h"
#include "sys/amd64/hw/io.h"
#include "sys/amd64/syscall.h"
#include "sys/binfmt_elf.h"
#include "sys/amd64/cpu.h"
#include "sys/config.h"
#include "sys/fs/ext2.h"
#include "sys/vmalloc.h"
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

extern uintptr_t argp_copy(struct thread *thr, const char *const argv[], size_t *arg_count);

static int user_init_start(const char *init_filename) {
    struct ofile fd;
    struct stat st;
    struct thread *user_init;
    char *file_buffer = NULL;
    mm_space_t space;
    ssize_t bread;
    int res;

    if ((res = vfs_stat(kernel_ioctx, init_filename, &st)) != 0) {
        kerror("%s: %s\n", init_filename, kstrerror(res));
        return -1;
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

    if ((res = elf_load(user_init, kernel_ioctx, &fd)) != 0) {
        panic("Failed to load ELF binary: %s\n", kstrerror(res));
    }

    vfs_close(kernel_ioctx, &fd);

    // Setup arguments for init task
    const char *const argp[] = {
        init_filename,
        NULL
    };
    uintptr_t argp_page;
    size_t argc;
    _assert((argp_page = argp_copy(user_init, argp, &argc)) != MM_NADDR);

    // Map arguments physical page into userspace
    uintptr_t argp_virt = vmfind(user_init->space, 0x100000, 0xF0000000, 1);
    _assert(argp_virt != MM_NADDR);
    // Map it as non-writable user-accessable
    _assert(amd64_map_single(user_init->space, argp_virt, argp_page, (1 << 2)) == 0);
    // Fix up the pointers
    uintptr_t *argp_fixup = (uintptr_t *) MM_VIRTUALIZE(argp_page);
    for (size_t i = 0; i < argc; ++i) {
        argp_fixup[i] += argp_virt;
    }

    struct cpu_context *ctx = (struct cpu_context *) user_init->data.rsp0;
    ctx->rdi = argp_virt;

    // Setup I/O context for init task
    struct ofile *stdin, *stdout, *stderr;

    stdin = kmalloc(sizeof(struct ofile));
    _assert(stdin);
    stdout = kmalloc(sizeof(struct ofile));
    _assert(stdout);
    stderr = stdout;

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

    user_init->fds[0] = stdin;
    user_init->fds[1] = stdout;
    user_init->fds[2] = stderr;

    user_init->ioctx.uid = 0;
    user_init->ioctx.gid = 0;
    user_init->ioctx.cwd_vnode = NULL;

    sched_add(user_init);

    return 0;
}

void init_func(void *arg) {
    const char *root_dev_name = (const char *) kernel_config[CFG_ROOT];
    const char *root_fallback = "ram0";
    const char *init_filename =
        (const char *) kernel_config[!strncmp(root_dev_name, "ram", 3) ? CFG_RDINIT : CFG_INIT];
    const char *init_fallback = "/init";

    struct vnode *root_dev;
    int res;

    // XXX XXX XXX: ext2 is broken, lol
    // Reads for large files fuck up
    //ext2_class_init();

    // Be [init] now
    thread_set_name(get_cpu()->thread, "init");

    // Create an alias for root device
    if ((res = dev_find(DEV_CLASS_BLOCK, root_dev_name, &root_dev)) != 0) {
        kerror("Failed to find root device: %s\n", root_dev_name);
        root_dev = NULL;

        if (strcmp(root_dev_name, root_fallback)) {
            // Try fallback root option
            if ((res = dev_find(DEV_CLASS_BLOCK, root_fallback, &root_dev)) != 0) {
                kerror("Failed to find root device: %s\n", root_fallback);
                root_dev = NULL;
            }
        }
    }

    if (!root_dev) {
        panic("No root filesystem\n");
    }

    dev_add_link("root", root_dev);

    if (!strncmp(root_dev_name, "ram", 3)) {
        // Check if device is ram-based, if so, try mounting using "ustar"
        if ((res = vfs_mount(kernel_ioctx, "/", root_dev->dev, "ustar", NULL)) != 0) {
            panic("Failed to mount root device using tarfs: %s\n", kstrerror(res));
        }
    } else {
        // Try "auto" option
        if ((res = vfs_mount(kernel_ioctx, "/", root_dev->dev, NULL, NULL)) != 0) {
            panic("Failed to mount root device: %s\n", kstrerror(res));
        }
    }

    // Start user init binary
    int init_success = 0;
    if (user_init_start(init_filename) != 0) {
        // User init failed, try /init
        if (strcmp(init_filename, init_fallback)) {
            if (user_init_start(init_fallback) == 0) {
                init_success = 1;
                kinfo("Using fallback \"%s\"\n", init_fallback);
            }
        }
    } else {
        init_success = 1;
        kinfo("Using \"%s\" as init\n", init_filename);
    }
    if (!init_success) {
        panic("No working init found\n");
    }

    outb(0x20, 0x20);
    outb(0xA0, 0x20);

    struct thread *this = get_cpu()->thread;
    this->exit_code = 0;
    this->flags |= THREAD_STOPPED;
    amd64_syscall_yield_stopped();
}
