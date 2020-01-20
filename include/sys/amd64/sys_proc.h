#pragma once
#include "sys/types.h"

void sys_exit(int status);
int sys_kill(int pid, int signum);
int sys_waitpid(int pid, int *status);
int sys_getpid(void);
int sys_setuid(uid_t uid);
int sys_setgid(gid_t gid);
uid_t sys_getuid(void);
gid_t sys_getgid(void);
pid_t sys_getpgid(pid_t pid);
int sys_setpgid(pid_t pid, pid_t pgrp);

// Non-compliant with linux style, but fuck'em, it just works
void sys_signal(uintptr_t entry);
void sys_sigret(void);
