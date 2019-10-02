/** vim: set ft=cpp.doxygen :
 * @file sys/syscall.h
 * @brief System call interface definitions
 */
#pragma once
#include "sys/types.h"

// Defined here so no need to include mm.h
#define userspace

// System call numbers

#define SYS_NR_READ         0
#define SYS_NR_WRITE        1
