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
#include "sys/reboot.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/rtc.h"
#include "sys/amd64/mm/map.h"
#include "sys/fs/pty.h"
#include "sys/time.h"

uint64_t syscall_count;

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
static int sys_nanosleep(const struct timespec *req, struct timespec *rem);
static int sys_gettimeofday(struct timeval *tv, struct timezone *tz);
static int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg);
static int sys_access(const char *path, int mode);
static int sys_waitpid(int pid, int *status);

// TODO: const struct termios *termp, const struct winsize *winp
static int sys_openpty(int *master, int *slave);

extern int sys_execve(const char *filename, const char *const argv[], const char *const envp[]);

// Non-compliant with linux style, but fuck'em, it just works
//static void sys_signal(uintptr_t entry);
static void sys_sigret(void);

__attribute__((noreturn)) void amd64_syscall_yield_stopped(void);

static int sys_getpid(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return thr->pid;
}

static void sys_signal(uintptr_t entry) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    kdebug("%u set its signal handler to %p\n", thr->pid, entry);
    thr->sigentry = entry;
}

const void *amd64_syscall_jmp_table[256] = {
    // File
    [SYSCALL_NR_READ] = sys_read,
    [SYSCALL_NR_WRITE] = sys_write,
    [SYSCALL_NR_READDIR] = sys_readdir,
    [SYSCALL_NR_OPEN] = sys_open,
    [SYSCALL_NR_CLOSE] = sys_close,
    [SYSCALL_NR_GETCWD] = sys_getcwd,
    [SYSCALL_NR_CHDIR] = sys_chdir,
    [SYSCALL_NR_STAT] = sys_stat,
    [SYSCALL_NR_ACCESS] = sys_access,

    // Terminal
    [SYSCALL_NRX_OPENPTY] = sys_openpty,

    // Process
    [SYSCALL_NR_EXIT] = sys_exit,
    [SYSCALL_NR_KILL] = sys_kill,
    [SYSCALL_NR_EXECVE] = sys_execve,
    [SYSCALL_NR_GETPID] = sys_getpid,
    [SYSCALL_NRX_SIGRET] = sys_sigret,
    [SYSCALL_NRX_SIGNAL] = sys_signal,
    [SYSCALL_NRX_WAITPID] = sys_waitpid,

    // Time
    [SYSCALL_NR_NANOSLEEP] = sys_nanosleep,
    [SYSCALL_NR_GETTIMEOFDAY] = sys_gettimeofday,

    // Memory
    [SYSCALL_NR_BRK] = sys_brk,

    // System
    [SYSCALL_NR_REBOOT] = sys_reboot,
    NULL
};

static int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg) {
    if (magic1 != (int) YGG_REBOOT_MAGIC1 || magic2 != (int) YGG_REBOOT_MAGIC2) {
        return -EINVAL;
    }

    switch (cmd) {
    case YGG_REBOOT_RESTART:
    case YGG_REBOOT_POWER_OFF:
    case YGG_REBOOT_HALT:
        sched_reboot(cmd);
        return 0;
    default:
        return -EINVAL;
    }
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
    char tmpbuf[512];
    size_t len;

    vfs_vnode_path(tmpbuf, thr->ioctx.cwd_vnode);
    len = strlen(tmpbuf);

    // TODO: actually implement this
    if (lim < len + 1) {
        return -1;
    }

    strcpy(buf, tmpbuf);

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

static int sys_openpty(int *master, int *slave) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    // Find two free file descriptors
    int fd_master = -1, fd_slave = -1;
    int res;

    for (int i = 0; i < 4; ++i) {
        if (!thr->fds[i].vnode) {
            if (fd_master == -1) {
                fd_master = i;
                continue;
            }
            fd_slave = i;
            break;
        }
    }

    if (fd_master == -1 || fd_slave == -1) {
        return -EMFILE;
    }

    struct ofile *of_master = &thr->fds[fd_master];
    struct ofile *of_slave = &thr->fds[fd_slave];

    struct pty *pty = pty_create();
    _assert(pty);

    of_master->vnode = pty->master;
    of_master->flags = O_RDWR;
    of_master->pos = 0;

    of_slave->vnode = pty->slave;
    of_slave->flags = O_RDWR;
    of_slave->pos = 0;

    *master = fd_master;
    *slave = fd_slave;

    return 0;
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

static int sys_access(const char *path, int mode) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    return vfs_access(&thr->ioctx, path, mode);
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
    thr->exit_code = status;
    thr->flags |= THREAD_STOPPED;
    amd64_syscall_yield_stopped();
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

static int sys_nanosleep(const struct timespec *req, struct timespec *rem) {
    struct thread *cur_thread = get_cpu()->thread;
    _assert(cur_thread);

    uint64_t time_now = system_time;
    uint64_t deadline = time_now + req->tv_sec * 1000000000ULL + req->tv_nsec;

    // Make sure scheduler won't select a waiting thread
    while (1) {
        // TODO: interruptible sleep
        if (system_time >= deadline) {
            break;
        }
        asm volatile ("sti; hlt; cli");
    }

    return 0;
}

static int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tz) {
        tz->tz_dsttime = 0;
        tz->tz_minuteswest = 0;
    }

    uint64_t secs = system_time / 1000000000ULL;

    struct rtc_time time_rtc;
    rtc_read(&time_rtc);
    secs += time_rtc.second + time_rtc.minute * 60 + time_rtc.hour * 3600;

    // System time is in nanos
    // TODO: use RTC-provided time for full system time
    tv->tv_usec = (system_time / 1000) % 1000000;
    tv->tv_sec = secs;

    return 0;
}

static void sys_sigret(void) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    thr->flags |= THREAD_SIGRET;

    asm volatile ("sti; hlt; cli");

    panic("Fuck\n");
}

static int sys_waitpid(int pid, int *status) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    // Find child with such PID
    struct thread *waited = NULL;
    int wait_good = 0;

    for (struct thread *child = thr->child; child; child = child->next_child) {
        if (!(child->flags & THREAD_KERNEL) && (int) child->pid == pid) {
            waited = child;
            break;
        }
    }

    if (!waited) {
        return -ESRCH;
    }

    while (1) {
        if (waited->flags & THREAD_STOPPED) {
            kdebug("%d is done waiting for %d\n", thr->pid, waited->pid);
            wait_good = 1;
            break;
        }
        asm volatile ("sti; hlt; cli");
    }

    if (wait_good) {
        if (status) {
            *status = waited->exit_code;
        }
        waited->flags |= THREAD_DONE_WAITING;

        thread_terminate(waited);
    }

    return 0;
}
