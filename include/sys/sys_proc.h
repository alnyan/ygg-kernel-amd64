#pragma once
#include "sys/types.h"

struct user_stack;

int sys_kill(pid_t pid, int signum);
void sys_exit(int status);
void sys_sigentry(uintptr_t entry);
void sys_sigreturn(void);
int sys_clone(int (*entry) (void *), void *stack, int flags, void *arg);
int sys_execve(const char *path, const char **argp, const char **envp);
pid_t sys_getpid(void);
pid_t sys_getppid(void);

uid_t sys_getuid(void);
gid_t sys_getgid(void);
int sys_setuid(uid_t uid);
int sys_setgid(gid_t gid);

int sys_sigaltstack(const struct user_stack *ss, struct user_stack *old_ss);

void sys_yield(void);

int sys_waitpid(pid_t pid, int *status, int flags);
pid_t sys_getpgid(pid_t pid);
int sys_setpgid(pid_t pid, pid_t pgrp);
