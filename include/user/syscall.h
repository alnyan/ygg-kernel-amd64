#pragma once

#define SYSCALL_NR_READ             0
#define SYSCALL_NR_WRITE            1
#define SYSCALL_NR_OPENAT           2
#define SYSCALL_NR_CLOSE            3
#define SYSCALL_NR_FSTATAT          4
#define SYSCALL_NR_LSEEK            8
#define SYSCALL_NR_MMAP             9
#define SYSCALL_NR_MUNMAP           11
#define SYSCALL_NRX_SBRK            12
#define SYSCALL_NR_IOCTL            16
#define SYSCALL_NR_FACCESSAT        21
#define SYSCALL_NR_PIPE             22
#define SYSCALL_NR_SELECT           23
#define SYSCALL_NR_DUP              32
#define SYSCALL_NR_DUP2             33
#define SYSCALL_NR_TRUNCATE         76
#define SYSCALL_NR_FTRUNCATE        77
#define SYSCALL_NR_GETCWD           79
#define SYSCALL_NR_CHDIR            80
#define SYSCALL_NR_MKDIRAT          83
#define SYSCALL_NR_CREAT            85
#define SYSCALL_NR_UNLINKAT         87
#define SYSCALL_NR_READDIR          89
#define SYSCALL_NR_CHMOD            90
#define SYSCALL_NR_CHOWN            92
#define SYSCALL_NR_MKNOD            133

#define SYSCALL_NR_NANOSLEEP        35
#define SYSCALL_NR_UNAME            63
#define SYSCALL_NR_GETTIMEOFDAY     96
#define SYSCALL_NR_SYNC             162
#define SYSCALL_NR_MOUNT            165
#define SYSCALL_NRX_UMOUNT          166
#define SYSCALL_NR_REBOOT           169
#define SYSCALL_NRX_MODULE_LOAD     175
#define SYSCALL_NRX_MODULE_UNLOAD   176

#define SYSCALL_NRX_SIGENTRY        13
#define SYSCALL_NR_SIGRETURN        15
#define SYSCALL_NR_YIELD            24
#define SYSCALL_NR_GETPID           39
#define SYSCALL_NR_CLONE            56
#define SYSCALL_NR_FORK             57
#define SYSCALL_NR_EXECVE           59
#define SYSCALL_NR_EXIT             60
#define SYSCALL_NR_KILL             62
#define SYSCALL_NR_GETUID           102
#define SYSCALL_NR_GETGID           104
#define SYSCALL_NR_SETUID           105
#define SYSCALL_NR_SETGID           106
#define SYSCALL_NR_SETPGID          109
#define SYSCALL_NR_GETPPID          110
#define SYSCALL_NR_GETPGID          121
#define SYSCALL_NR_SIGALTSTACK      131
#define SYSCALL_NRX_WAITPID         247

#define SYSCALL_NR_SOCKET           41
#define SYSCALL_NR_CONNECT          42
#define SYSCALL_NR_ACCEPT           43
#define SYSCALL_NR_SENDTO           44
#define SYSCALL_NR_RECVFROM         45
#define SYSCALL_NR_BIND             49
#define SYSCALL_NR_SETSOCKOPT       54
#define SYSCALL_NRX_NETCTL          248

#define SYSCALL_NRX_TRACE           249
