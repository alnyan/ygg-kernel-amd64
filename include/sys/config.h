#pragma once
#include "sys/types.h"

#define KERNEL_CMDLINE_MAX 256

enum {
    CFG_ROOT = 1,
    CFG_INIT,
    CFG_RDINIT,
    CFG_CONSOLE,
    CFG_DEBUG,

    __CFG_SIZE
};

// Simple configuration array for the kernel
extern uintptr_t kernel_config[__CFG_SIZE];
extern char g_kernel_cmdline[KERNEL_CMDLINE_MAX];

// Configure kernel from cmdline
void kernel_set_cmdline(char *cmdline);
