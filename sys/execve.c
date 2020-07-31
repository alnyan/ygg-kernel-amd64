#include "arch/amd64/mm/pool.h"
#include "arch/amd64/context.h"
#include "arch/amd64/mm/map.h"
#include "sys/mem/vmalloc.h"
#include "arch/amd64/cpu.h"
#include "sys/binfmt_elf.h"
#include "sys/sys_proc.h"
#include "sys/mem/phys.h"
#include "user/errno.h"
#include "user/fcntl.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"
#include "fs/vfs.h"
#include "sys/mm.h"

// Discontiguous-destination range copy,
// XXX: may have been more efficient
static size_t procv_strcpy_paged(uintptr_t *phys_pages,
                                 size_t offset,
                                 const char *src,
                                 size_t page_count) {
    size_t ncpy = strlen(src) + 1;
    size_t off_in_str = 0;

    while (ncpy) {
        size_t off_in_page = offset % MM_PAGE_SIZE;
        size_t page_index = offset / MM_PAGE_SIZE;
        _assert(page_index < page_count);
        size_t len = MIN(ncpy, MM_PAGE_SIZE - off_in_page);

        void *dst = (void *) MM_VIRTUALIZE(phys_pages[page_index] + off_in_page);
        memcpy(dst, src + off_in_str, len);

        off_in_str += len;
        offset += len;
        ncpy -= len;
    }

    return off_in_str;
}

// Setup process vectors:
// argp, envp, auxv
// TODO: elf auxv

static int procv_setup(const char *const argv[],
                       const char *const envp[],
                       uintptr_t *phys_pages,
                       uintptr_t *vecp,
                       size_t *procv_page_count) {
#define PTRS_PER_PAGE   (MM_PAGE_SIZE / sizeof(uintptr_t))
#define NEW_ARGV(i)     (*((uintptr_t *) MM_VIRTUALIZE(phys_pages[i / PTRS_PER_PAGE]) + \
                         i % PTRS_PER_PAGE))
#define NEW_ENVP(i)     (*({ \
                            size_t __i0 = i + (argc + 1); \
                            ((uintptr_t *) MM_VIRTUALIZE(phys_pages[__i0 / PTRS_PER_PAGE]) + \
                             __i0 % PTRS_PER_PAGE); \
                        }))

    // TODO: store pointers and data on separate pages

    size_t page_count;
    size_t offset;
    size_t argc, envc;

    // Count the arguments
    for (argc = 0; argv[argc]; ++argc);
    for (envc = 0; envp[envc]; ++envc);

    // Skip space for pointer arrays
    offset = (argc + 1) * sizeof(uintptr_t);
    offset += (envc + 1) * sizeof(uintptr_t);
    // Calculate total envp + argv length
    for (size_t i = 0; i < argc; ++i) {
        offset += strlen(argv[i]) + 1;
    }
    for (size_t i = 0; i < envc; ++i) {
        offset += strlen(envp[i]) + 1;
    }

    // Allocate pages for data
    // TODO: is it possible to somehow use CoW here?
    page_count = (offset + MM_PAGE_SIZE - 1) / MM_PAGE_SIZE;
    if (page_count > *procv_page_count) {
        // Can't store all the physical pages in provided array ptr
        return -ENOMEM;
    }

    *procv_page_count = page_count;
    for (size_t i = 0; i < page_count; ++i) {
        phys_pages[i] = mm_phys_alloc_page();
        _assert(phys_pages[i] != MM_NADDR);
    }

    // Copy text data
    offset = (argc + envc + 2) * sizeof(uintptr_t);
    for (size_t i = 0; i < argc; ++i) {
        NEW_ARGV(i) = offset;
        offset += procv_strcpy_paged(phys_pages, offset, argv[i], page_count);
    }
    for (size_t i = 0; i < envc; ++i) {
        NEW_ENVP(i) = offset;
        offset += procv_strcpy_paged(phys_pages, offset, envp[i], page_count);
    }

    vecp[0] = argc;
    vecp[1] = envc;
    vecp[2] = offset;
    return 0;
#undef PTRS_PER_PAGE
}

int sys_execve(const char *path, const char **argv, const char **envp) {
    struct thread *thr = thread_self;
    _assert(thr);
    struct process *proc = thr->proc;
    _assert(proc);
    char shebang[128];

    if (proc->thread_count > 1) {
        panic("XXX: execve() in multithreaded process\n");
    }

    struct ofile fd = {0};
    struct stat st;
    uintptr_t entry;
    size_t argc;
    int res;
    int was_kernel = 0;

    if ((res = vfs_fstatat(&proc->ioctx, NULL, path, &st, 0)) != 0) {
        kerror("execve(%s): %s\n", path, kstrerror(res));
        return res;
    }

    if ((res = vfs_openat(&proc->ioctx, &fd, NULL, path, O_RDONLY, 0)) != 0) {
        kerror("%s: %s\n", path, kstrerror(res));
        return res;
    }

    if ((res = vfs_read(&proc->ioctx, &fd, shebang, sizeof(shebang))) <= 0) {
        kerror("%s: %s\n", path, kstrerror(res));
        return res;
    }

    if (!binfmt_is_elf(shebang, res)) {
        // Try checking for shebang line
        if (shebang[0] == '#' && shebang[1] == '!') {
            char *e = strchr(shebang, '\n');
            if (!e) {
                vfs_close(&proc->ioctx, &fd);
                return -EINVAL;
            }
            *e = 0;

            int argc;
            for (argc = 0; argv[argc]; ++argc);
            // For interpreter
            ++argc;
            // Sanity check
            _assert(argc <= 32);
            const char *argv_new[argc + 1];
            argv_new[0] = shebang + 2;
            argv_new[1] = path;
            for (int i = 1; i < argc; ++i) {
                argv_new[i + 1] = argv[i];
            }
            argv_new[argc] = NULL;

            return sys_execve(shebang + 2, argv_new, envp);
        }
    } else {
        int is_dynamic = 0;
        if ((res = elf_is_dynamic(&proc->ioctx, &fd, &is_dynamic)) != 0) {
            kerror("%s: %s\n", path, kstrerror(res));
            vfs_close(&proc->ioctx, &fd);
            return -EINVAL;
        }

        if (is_dynamic) {
            vfs_close(&proc->ioctx, &fd);

            int argc;
            for (argc = 0; argv[argc]; ++argc);

            const char *argv_new[argc + 3];
            argv_new[0] = "/lib/ld";
            argv_new[1] = path;
            for (int i = 0; i < argc; ++i) {
                argv_new[i + 2] = argv[i];
            }
            argv_new[argc + 2] = NULL;

            return sys_execve(argv_new[0], argv_new, envp);
        }
    }

    const char *e = strrchr(path, '/');
    const char *name = e + 1;
    if (!e) {
        name = path;
    }
    size_t name_len = MIN(strlen(name), sizeof(proc->name) - 1);
    strncpy(proc->name, name, name_len);
    proc->name[name_len] = 0;

    // Copy args
    _assert(argv);
    _assert(envp);
    // 128K of argp/envp data
    // 256 bytes of stack here
#define PROCV_MAX_PAGES     32
    uintptr_t procv_phys_pages[PROCV_MAX_PAGES];
    size_t procv_page_count = PROCV_MAX_PAGES;
    // [0] - argc
    // [1] - envc
    // [2] - full size
    uintptr_t procv_vecp[3];

    if (procv_setup(argv, envp, procv_phys_pages, procv_vecp, &procv_page_count) != 0) {
        panic("Failed to copy argp/envp to new process\n");
    }

    if (proc->space == mm_kernel) {
        was_kernel = 1;
        // Have to allocate a new PID for kernel -> userspace transition
        proc->pid = process_alloc_pid(1); //thread_alloc_pid(1);
        proc->pgid = proc->pid;

        // Have to remove parent/child relation for transition
        _assert(!proc->first_child);
        if (proc->parent) {
            panic("NYI\n");
        }
        proc->first_child = NULL;
        proc->next_child = NULL;
        // TODO: automatically parent all subsequent processes to init (1)
        proc->parent = NULL;
        proc->sigq = 0;

        proc->space = amd64_mm_pool_alloc();
        proc->flags = 0;
        _assert(proc->space);

        mm_space_clone(proc->space, mm_kernel, MM_CLONE_FLG_KERNEL);

        // Setup main thread
        asm volatile ("cli");
        thr->data.fxsave = kmalloc(FXSAVE_REGION);
        _assert(thr->data.fxsave);

        thr->data.cr3 = MM_PHYS(proc->space);
        // Switch CR3 to the newly allocated space!
        asm volatile ("movq %0, %%rax; movq %%rax, %%cr3"::"a"(thr->data.cr3));
        asm volatile ("sti");
    } else {
        mm_space_release(proc);
    }

    if ((res = elf_load(proc, &proc->ioctx, &fd, &entry)) != 0) {
        vfs_close(&proc->ioctx, &fd);

        kerror("elf load failed: %s\n", kstrerror(res));
        sys_exit(-1);

        panic("This code shouldn't run\n");
    }

    vfs_close(&proc->ioctx, &fd);

    // Close O_CLOEXEC files
    for (size_t i = 0; i < THREAD_MAX_FDS; ++i) {
        if (proc->fds[i] && (proc->fds[i]->flags & OF_CLOEXEC)) {
            ofile_close(&proc->ioctx, proc->fds[i]);
            proc->fds[i] = NULL;
        }
    }

    // Allocate a virtual address to map argp page
    uintptr_t procv_virt = vmfind(proc->space, 0x100000, 0xF0000000, procv_page_count);
    _assert(procv_virt != MM_NADDR);
    for (size_t i = 0; i < procv_page_count; ++i) {
        _assert(mm_map_single(proc->space,
                              procv_virt + i * MM_PAGE_SIZE,
                              procv_phys_pages[i],
                              MM_PAGE_USER | MM_PAGE_WRITE,
                              PU_PRIVATE) == 0);
    }
    uintptr_t *argv_fixup = (uintptr_t *) procv_virt;
    uintptr_t *envp_fixup = (uintptr_t *) procv_virt + procv_vecp[0] + 1;

    for (size_t i = 0; i < procv_vecp[0]; ++i) {
        argv_fixup[i] += procv_virt;
    }
    for (size_t i = 0; i < procv_vecp[1]; ++i) {
        envp_fixup[i] += procv_virt;
    }
    // Terminate arrays
    argv_fixup[procv_vecp[0]] = 0;
    envp_fixup[procv_vecp[1]] = 0;

    thr->signal_entry = MM_NADDR;
    thr->signal_stack_base = MM_NADDR;
    thr->signal_stack_size = 0;

    thr->data.rsp0 = thr->data.rsp0_top;

    // Allocate a new user stack
    uintptr_t ustack = vmalloc(proc->space, 0x100000, 0xF0000000, 4, MM_PAGE_USER | MM_PAGE_WRITE /* | MM_PAGE_NOEXEC */, PU_PRIVATE);
    thr->data.rsp3_base = ustack;
    thr->data.rsp3_size = 4 * MM_PAGE_SIZE;

    if (was_kernel) {
        proc_add_entry(proc);
    }

    // Up to 4095 argc and envc
    _assert(procv_vecp[0] < 4096);
    _assert(procv_vecp[2] < 4096);
    uintptr_t arg = procv_vecp[0] | (procv_vecp[2] << 12) | ((uintptr_t) argv_fixup << 12);
    context_exec_enter(arg, thr, thr->data.rsp3_base + thr->data.rsp3_size - 8, entry);

    panic("This code shouldn't run\n");
}

