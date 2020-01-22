#pragma once
#include "sys/types.h"

enum {
    CFG_ROOT = 1,
    CFG_INIT,
    CFG_RDINIT,

    __CFG_SIZE
};

// Simple configuration array for the kernel
extern uintptr_t kernel_config[__CFG_SIZE];

// Configure kernel from cmdline
void kernel_set_cmdline(char *cmdline);
