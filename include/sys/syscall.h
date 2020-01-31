#pragma once

#define SYSCALL_NR_READ         0
#define SYSCALL_NR_WRITE        1

#define SYSCALL_NRX_SIGENTRY    13
void sys_sigentry(uintptr_t entry);
#define SYSCALL_NR_SIGRETURN    15
void sys_sigreturn(void);
#define SYSCALL_NRX_SUICIDE     253
void sys_suicide(void);

#define SYSCALL_NR_FORK         57
int sys_fork(void *ctx);
#define SYSCALL_NR_EXIT         60
__attribute__((noreturn)) void sys_exit(int status);

#define SYSCALL_NR_DEBUG_SLEEP  254
void sys_debug_sleep(uint64_t ms);
#define SYSCALL_NR_DEBUG_TRACE  255
void sys_debug_trace(const char *msg);
