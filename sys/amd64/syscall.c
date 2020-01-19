#include "sys/amd64/syscall.h"
#include "sys/syscall.h"

#include "sys/amd64/sys_file.h"
#include "sys/amd64/sys_proc.h"
#include "sys/amd64/sys_sys.h"
#include "sys/amd64/sys_mem.h"

uint64_t syscall_count;

extern int sys_execve(const char *filename, const char *const argv[], const char *const envp[]);

const void *amd64_syscall_jmp_table[256] = {
    // File
    [SYSCALL_NR_READ] = sys_read,
    [SYSCALL_NR_WRITE] = sys_write,
    [SYSCALL_NR_READDIR] = sys_readdir,
    [SYSCALL_NR_OPEN] = sys_open,
    [SYSCALL_NR_CLOSE] = sys_close,
    [SYSCALL_NR_LSEEK] = sys_lseek,
    [SYSCALL_NR_SELECT] = sys_select,
    [SYSCALL_NR_GETCWD] = sys_getcwd,
    [SYSCALL_NR_CHDIR] = sys_chdir,
    [SYSCALL_NR_STAT] = sys_stat,
    [SYSCALL_NR_ACCESS] = sys_access,
    [SYSCALL_NRX_OPENPTY] = sys_openpty,
    [SYSCALL_NR_UNLINK] = sys_unlink,
    [SYSCALL_NR_MKDIR] = sys_mkdir,
    [SYSCALL_NR_CREAT] = sys_creat,
    [SYSCALL_NR_RMDIR] = sys_rmdir,
    [SYSCALL_NR_CHMOD] = sys_chmod,
    [SYSCALL_NR_CHOWN] = sys_chown,
    [SYSCALL_NRX_ISATTY] = sys_isatty,

    // Process
    [SYSCALL_NR_EXIT] = sys_exit,
    [SYSCALL_NR_KILL] = sys_kill,
    [SYSCALL_NR_EXECVE] = sys_execve,
    [SYSCALL_NR_GETPID] = sys_getpid,
    [SYSCALL_NRX_SIGRET] = sys_sigret,
    [SYSCALL_NRX_SIGNAL] = sys_signal,
    [SYSCALL_NRX_WAITPID] = sys_waitpid,
    [SYSCALL_NR_SETUID] = sys_setuid,
    [SYSCALL_NR_SETGID] = sys_setgid,
    [SYSCALL_NR_GETUID] = sys_getuid,
    [SYSCALL_NR_GETGID] = sys_getgid,

    // System
    [SYSCALL_NR_UNAME] = sys_uname,
    [SYSCALL_NR_MOUNT] = sys_mount,
    [SYSCALL_NRX_UMOUNT] = sys_umount,
    [SYSCALL_NR_REBOOT] = sys_reboot,
    [SYSCALL_NR_NANOSLEEP] = sys_nanosleep,
    [SYSCALL_NR_GETTIMEOFDAY] = sys_gettimeofday,

    // Memory
    [SYSCALL_NR_BRK] = sys_brk,
};

