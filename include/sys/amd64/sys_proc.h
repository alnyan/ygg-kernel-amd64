#pragma once
#include "sys/types.h"

void sys_exit(int status);
int sys_kill(int pid, int signum);
int sys_waitpid(int pid, int *status);
int sys_getpid(void);

// Non-compliant with linux style, but fuck'em, it just works
void sys_signal(uintptr_t entry);
void sys_sigret(void);
