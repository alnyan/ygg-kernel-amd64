#pragma once

#define SYSCALL_NR_READ     0
#define SYSCALL_NR_WRITE    1
#define SYSCALL_NR_OPEN     2
#define SYSCALL_NR_CLOSE    3
#define SYSCALL_NR_STAT     4
#define SYSCALL_NR_GETCWD   79
#define SYSCALL_NR_CHDIR    80
#define SYSCALL_NR_READDIR  89

#define SYSCALL_NR_BRK      12
#define SYSCALL_NR_GETPID   39
#define SYSCALL_NRX_SIGNAL  48
#define SYSCALL_NR_FORK     57
#define SYSCALL_NR_EXECVE   59
#define SYSCALL_NR_EXIT     60
#define SYSCALL_NR_KILL     62
#define SYSCALL_NRX_SIGRET  119
