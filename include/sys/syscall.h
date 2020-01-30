#pragma once

#define SYSCALL_NR_FORK         57
int sys_fork(void *ctx);

#define SYSCALL_NR_EXIT         60
__attribute__((noreturn)) void sys_exit(int status);

#define SYSCALL_NR_DEBUG_TRACE  255
void sys_debug_trace(const char *msg);
