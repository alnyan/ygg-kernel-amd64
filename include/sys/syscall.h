#pragma once

#define SYSCALL_NR_READ         0
#define SYSCALL_NR_WRITE        1
#define SYSCALL_NR_OPEN         2
#define SYSCALL_NR_CLOSE        3
#define SYSCALL_NR_STAT         4
#define SYSCALL_NR_ACCESS       21
#define SYSCALL_NR_GETCWD       79
#define SYSCALL_NR_CHDIR        80
#define SYSCALL_NR_MKDIR        83
#define SYSCALL_NR_RMDIR        84
#define SYSCALL_NR_CREAT        85
#define SYSCALL_NR_UNLINK       87
#define SYSCALL_NR_READDIR      89

#define SYSCALL_NR_BRK          12
#define SYSCALL_NR_NANOSLEEP    35
#define SYSCALL_NR_GETPID       39
#define SYSCALL_NRX_SIGNAL      48
#define SYSCALL_NR_FORK         57
#define SYSCALL_NR_EXECVE       59
#define SYSCALL_NR_EXIT         60
#define SYSCALL_NR_KILL         62
#define SYSCALL_NR_GETTIMEOFDAY 96
#define SYSCALL_NR_GETUID       102
#define SYSCALL_NR_GETGID       104
#define SYSCALL_NR_SETUID       105
#define SYSCALL_NR_SETGID       106

#define SYSCALL_NR_MOUNT        165
#define SYSCALL_NR_REBOOT       169

#define SYSCALL_NRX_OPENPTY     118
#define SYSCALL_NRX_SIGRET      119
#define SYSCALL_NRX_WAITPID     247
