#pragma once
#include "bits/syscall.h"
#include <sys/reboot.h>

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

void *sbrk(intptr_t diff);
int reboot(int magic1, int magic2, unsigned int cmd, void *arg);
