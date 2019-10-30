#pragma once
#include "bits/syscall.h"

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

void *sbrk(intptr_t diff);
