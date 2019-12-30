#include "sys/amd64/cpu.h"
#include "sys/syscall.h"
#include "sys/thread.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/sched.h"
#include "sys/types.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/fs/vfs.h"
#include "sys/fcntl.h"
#include "sys/tty.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/map.h"

static ssize_t sys_read(int fd, void *buf, size_t lim);
static ssize_t sys_write(int fd, const void *buf, size_t lim);
// TODO: make this compatible with linux' sys_old_readdir
static ssize_t sys_readdir(int fd, struct dirent *ent);
static int sys_open(const char *filename, int flags, int mode);
static void sys_close(int fd);
static int sys_getcwd(char *buf, size_t lim);
static int sys_chdir(const char *path);
static int sys_stat(const char *filename, struct stat *st);
static void sys_exit(int status);
static int sys_kill(int pid, int signum);
static int sys_brk(void *ptr);

extern int sys_execve(const char *filename, const char *const argv[], const char *const envp[]);

// Non-compliant with linux style, but fuck'em, it just works
static void sys_signal(uintptr_t entry);
static void sys_sigret(void);

__attribute__((noreturn)) void amd64_syscall_yield_stopped(void);

intptr_t amd64_syscall(uintptr_t rdi, uintptr_t rsi, uintptr_t rdx, uintptr_t rcx, uintptr_t r10, uintptr_t rax) {
    asm volatile ("cli");
    switch (rax) {
    case SYSCALL_NR_READ:
        return sys_read((int) rdi, (void *) rsi, (size_t) rdx);
    case SYSCALL_NR_WRITE:
        return sys_write((int) rdi, (const void *) rsi, (size_t) rdx);
    case SYSCALL_NR_OPEN:
        return sys_open((const char *) rdi, (int) rsi, (int) rdx);
    case SYSCALL_NR_CLOSE:
        sys_close((int) rdi);
        return 0;
    case SYSCALL_NR_STAT:
        return sys_stat((const char *) rdi, (struct stat *) rsi);
    case SYSCALL_NR_EXECVE:
        return sys_execve((const char *) rdi, NULL, NULL);
    case SYSCALL_NR_READDIR:
        return sys_readdir((int) rdi, (struct dirent *) rsi);
    case SYSCALL_NR_GETCWD:
        return sys_getcwd((char *) rdi, (size_t) rsi);
    case SYSCALL_NR_CHDIR:
        return sys_chdir((const char *) rdi);

    case SYSCALL_NRX_SIGNAL:
        sys_signal(rdi);
        return 0;
    case SYSCALL_NR_BRK:
        return sys_brk((void *) rdi);
    case SYSCALL_NR_GETPID:
        {
            struct thread *t = get_cpu()->thread;
            _assert(t);
            return t->pid;
        }
    case SYSCALL_NR_EXIT:
        sys_exit((int) rdi);
        amd64_syscall_yield_stopped();
    case SYSCALL_NR_KILL:
        return sys_kill((int) rdi, (int) rsi);
    case SYSCALL_NRX_SIGRET:
        sys_sigret();
        return 0;

    default:
        kerror("unknown syscall: %u\n", rax);
        // TODO: I guess this should ideally crash the application
        //       with invalid opcode or something
        return -1;
    }
    return 0;
}

static ssize_t sys_read(int fd, void *buf, size_t lim) {
    if (fd >= 4 || fd < 0) {
        return -EBADF;
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    struct ofile *of = &thr->fds[fd];

    if (!of->vnode) {
        return -EBADF;
    }

    return vfs_read(&thr->ioctx, of, buf, lim);
}

static ssize_t sys_write(int fd, const void *buf, size_t lim) {
    if (fd >= 4 || fd < 0) {
        return -EBADF;
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    struct ofile *of = &thr->fds[fd];

    if (!of->vnode) {
        return -EBADF;
    }

    return vfs_write(&thr->ioctx, of, buf, lim);
}

static ssize_t sys_readdir(int fd, struct dirent *ent) {
    if (fd >= 4 || fd < 0) {
        return -EBADF;
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    struct ofile *of = &thr->fds[fd];

    if (!of->vnode) {
        return -EBADF;
    }

    struct dirent *src = vfs_readdir(&thr->ioctx, of);

    if (!src) {
        return -1;
    }

    // XXX: safe?
    memcpy(ent, src, sizeof(struct dirent) + strlen(src->d_name));

    return src->d_reclen;
}

static int sys_chdir(const char *filename) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(filename);

    return vfs_chdir(&thr->ioctx, filename);
}

// Kinda incompatible with linux, but who cares as long as it's
// POSIX on the libc side
static int sys_getcwd(char *buf, size_t lim) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    _assert(buf);

    // TODO: actually implement this
    if (lim < 4) {
        return -1;
    }

    strcpy(buf, "???");

    return 0;
}

static int sys_open(const char *filename, int flags, int mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    int fd = -1;

    for (int i = 0; i < 4; ++i) {
        if (!thr->fds[i].vnode) {
            fd = i;
            break;
        }
    }

    if (fd != -1) {
        struct ofile *of = &thr->fds[fd];

        int res = vfs_open(&thr->ioctx, of, filename, mode, flags);

        if (res != 0) {
            of->vnode = NULL;
            return res;
        }

        return fd;
    } else {
        return -1;
    }
}

static void sys_close(int fd) {
    if (fd >= 4 || fd < 0) {
        return;
    }
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    struct ofile *of = &thr->fds[fd];

    if (!of->vnode) {
        return;
    }

    vfs_close(&thr->ioctx, of);
    of->vnode = NULL;
}

static int sys_stat(const char *filename, struct stat *st) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    int res = vfs_stat(&thr->ioctx, filename, st);

    return res;
}

static int sys_brk(void *addr) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    uintptr_t image_end = thr->image.image_end;
    uintptr_t curbrk = thr->image.brk;

    if ((uintptr_t) addr < curbrk) {
        panic("TODO: negative sbrk\n");
    }

    if ((uintptr_t) addr >= 0x100000000) {
        // We don't like addr
        return -EINVAL;
    }

    // Map new pages in brk area
    uintptr_t addr_page_align = image_end & ~0xFFF;
    size_t brk_size = (uintptr_t) addr - image_end;
    size_t npages = (brk_size + 0xFFF) / 0x1000;
    uintptr_t page_phys;

    kdebug("brk uses %u pages from %p\n", npages, addr_page_align);

    for (size_t i = 0; i < npages; ++i) {
        if ((page_phys = amd64_map_get(thr->space, addr_page_align + (i << 12), NULL)) == MM_NADDR) {
            // Allocate a page here
            page_phys = amd64_phys_alloc_page();
            if (page_phys == MM_NADDR) {
                return -ENOMEM;
            }

            kdebug("%p: allocated a page\n", addr_page_align + (i << 12));

            if (amd64_map_single(thr->space, addr_page_align + (i << 12), page_phys, (1 << 1) | (1 << 2)) != 0) {
                amd64_phys_free(page_phys);
                return -ENOMEM;
            }
        } else {
            kdebug("%p: already present\n", addr_page_align + (i << 12));
        }
    }

    return 0;
}

static void sys_exit(int status) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    kdebug("%u exited with code %d\n", thr->pid, status);
    thr->flags |= THREAD_STOPPED;
}

static void sys_signal(uintptr_t entry) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    kdebug("%u set its signal handler to %p\n", thr->pid, entry);
    thr->sigentry = entry;
}

static int sys_kill(int pid, int signum) {
    asm volatile ("cli");
    struct thread *cur_thread = get_cpu()->thread;

    // Find destination thread
    struct thread *dst_thread = sched_find(pid);

    if (!dst_thread) {
        return -ESRCH;
    }

    // Push an item to signal queue
    thread_signal(dst_thread, signum);

    if (cur_thread == dst_thread) {
        // Suicide signal, just hang on and wait
        // until scheduler decides it's our time
        asm volatile ("sti; hlt; cli");

        kdebug("Returned from shit\n");
    }

    return 0;
}

extern void amd64_syscall_iretq(uintptr_t ctx);

static void sys_sigret(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    thr->flags |= THREAD_SIGRET;

    asm volatile ("sti; hlt; cli");

    panic("Fuck\n");
}
