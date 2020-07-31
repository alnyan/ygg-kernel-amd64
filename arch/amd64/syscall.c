#include <config.h>

#include "user/syscall.h"
#include "user/errno.h"
#include "user/time.h"
#include "sys/sched.h"
#include "arch/amd64/cpu.h"
#include "sys/thread.h"
#include "sys/debug.h"

#include "sys/mem/shmem.h"
#include "sys/sys_file.h"
#include "sys/sys_sys.h"
#include "sys/sys_proc.h"
#if defined(ENABLE_NET)
#include "sys/sys_net.h"
#endif
#include "sys/mod.h"

#define MSR_IA32_STAR               0xC0000081
#define MSR_IA32_LSTAR              0xC0000082
#define MSR_IA32_SFMASK             0xC0000084

extern void syscall_entry(void);

static void sys_debug_trace(const char *fmt, ...) {
    struct thread *thr = thread_self;
    _assert(thr);
    struct process *proc = thr->proc;
    _assert(proc);

    va_list args;
    userptr_check(fmt);
    debugf(DEBUG_DEFAULT, "[%2u] ", proc->pid);
    va_start(args, fmt);
    debugfv(DEBUG_DEFAULT, fmt, args);
    va_end(args);
}

void *syscall_table[256] = {
    // I/O
    [SYSCALL_NR_READ] =             sys_read,
    [SYSCALL_NR_WRITE] =            sys_write,
    [SYSCALL_NR_OPENAT] =           sys_openat,
    [SYSCALL_NR_CLOSE] =            sys_close,
    [SYSCALL_NR_FSTATAT] =          sys_fstatat,
    [SYSCALL_NR_LSEEK] =            sys_lseek,
    [SYSCALL_NR_MMAP] =             sys_mmap,
    [SYSCALL_NR_MUNMAP] =           sys_munmap,
    [SYSCALL_NR_IOCTL] =            sys_ioctl,
    [SYSCALL_NR_FACCESSAT] =        sys_faccessat,
    [SYSCALL_NR_PIPE] =             sys_pipe,
    [SYSCALL_NR_SELECT] =           sys_select,
    [SYSCALL_NR_DUP] =              sys_dup,
    [SYSCALL_NR_DUP2] =             sys_dup2,
    [SYSCALL_NR_TRUNCATE] =         sys_truncate,
    [SYSCALL_NR_FTRUNCATE] =        sys_ftruncate,
    [SYSCALL_NR_GETCWD] =           sys_getcwd,
    [SYSCALL_NR_CHDIR] =            sys_chdir,
    [SYSCALL_NR_MKDIRAT] =          sys_mkdirat,
    [SYSCALL_NR_CREAT] =            sys_creat,
    [SYSCALL_NR_UNLINKAT] =         sys_unlinkat,
    [SYSCALL_NR_READDIR] =          sys_readdir,
    [SYSCALL_NR_CHMOD] =            sys_chmod,
    [SYSCALL_NR_CHOWN] =            sys_chown,
    [SYSCALL_NR_MKNOD] =            sys_mknod,

    // User control
    [SYSCALL_NR_GETUID] =           sys_getuid,
    [SYSCALL_NR_GETGID] =           sys_getgid,
    [SYSCALL_NR_SETUID] =           sys_setuid,
    [SYSCALL_NR_SETGID] =           sys_setgid,
    // Process control
    [SYSCALL_NRX_SIGENTRY] =        sys_sigentry,
    [SYSCALL_NR_EXECVE] =           sys_execve,
    [SYSCALL_NR_YIELD] =            sys_yield,
    [SYSCALL_NR_GETPID] =           sys_getpid,
    [SYSCALL_NR_CLONE] =            sys_clone,
    [SYSCALL_NR_KILL] =             sys_kill,
    [SYSCALL_NR_EXIT] =             sys_exit,
    [SYSCALL_NR_SIGRETURN] =        sys_sigreturn,
    [SYSCALL_NRX_WAITPID] =         sys_waitpid,
    [SYSCALL_NR_GETPGID] =          sys_getpgid,
    [SYSCALL_NR_SETPGID] =          sys_setpgid,
    [SYSCALL_NR_SETSID] =           sys_setsid,
    [SYSCALL_NR_SIGALTSTACK] =      sys_sigaltstack,
    [SYSCALL_NR_GETPPID] =          sys_getppid,

    // System
    [SYSCALL_NR_SYNC] =             sys_sync,
    [SYSCALL_NR_MOUNT] =            sys_mount,
    [SYSCALL_NRX_UMOUNT] =          sys_umount,
    [SYSCALL_NR_UNAME] =            sys_uname,
    [SYSCALL_NR_NANOSLEEP] =        sys_nanosleep,
    [SYSCALL_NR_GETTIMEOFDAY] =     sys_gettimeofday,
    [SYSCALL_NR_REBOOT] =           sys_reboot,
    [SYSCALL_NRX_MODULE_LOAD] =     sys_module_load,
    [SYSCALL_NRX_MODULE_UNLOAD] =   sys_module_unload,

#if defined(ENABLE_NET)
    // Network
    [SYSCALL_NR_SOCKET] =           sys_socket,
    [SYSCALL_NR_CONNECT] =          sys_connect,
    [SYSCALL_NR_ACCEPT] =           sys_accept,
    [SYSCALL_NR_SENDTO] =           sys_sendto,
    [SYSCALL_NR_RECVFROM] =         sys_recvfrom,
    [SYSCALL_NR_BIND] =             sys_bind,
    [SYSCALL_NR_SETSOCKOPT] =       sys_setsockopt,
    [SYSCALL_NRX_NETCTL] =          sys_netctl,
#endif

    // Extension
    [SYSCALL_NRX_TRACE] =           sys_debug_trace,
};

int syscall_undefined(uint64_t rax) {
    debugs(DEBUG_DEFAULT, "\033[41m");
    if (sched_ready) {
        thread_dump(DEBUG_DEFAULT, thread_self);
    }
    debugf(DEBUG_DEFAULT, "Undefined system call: %d\n", rax);
    debugs(DEBUG_DEFAULT, "\033[0m");
    thread_signal(thread_self, SIGSYS);
    // TODO: more context?
    return -ENOSYS;
}

void syscall_init(void) {
    // LSTAR = syscall_entry
    wrmsr(MSR_IA32_LSTAR, (uintptr_t) syscall_entry);

    // SFMASK = (1 << 9) /* IF */
    wrmsr(MSR_IA32_SFMASK, (1 << 9));

    // STAR = ((ss3 - 8) << 48) | (cs0 << 32)
    wrmsr(MSR_IA32_STAR, ((uint64_t) (0x1B - 8) << 48) | ((uint64_t) 0x08 << 32));

    // Set SCE bit
    uint64_t efer = rdmsr(MSR_IA32_EFER);
    efer |= (1 << 0);
    wrmsr(MSR_IA32_EFER, efer);
}
