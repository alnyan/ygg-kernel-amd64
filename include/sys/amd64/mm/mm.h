/* vim: set ft=cpp.doxygen : */
#pragma once
#include <stdint.h>
#include "sys/amd64/loader/data.h"

/// The place where the kernel pages are virtually mapped to
#define KERNEL_VIRT_BASE        0xFFFFFF0000000000

/// amd64 standard states that addresses' upper bits are copies of the 47th bit, so these need to be
///  stripped down to only 48 bits
#define AMD64_MM_STRIPSX(a)         ((uintptr_t) (a) & 0xFFFFFFFFFFFF)
#define AMD64_MM_ADDRSX(a)          (((uintptr_t) (a) & (1ULL << 47)) ? \
                                        (0xFFFFFF0000000000 | ((uintptr_t) (a))) : \
                                        ((uintptr_t) (a)))

#define MM_VIRTUALIZE(a)            ((uintptr_t) (a) + 0xFFFFFF0000000000)
#define MM_PHYS(a)                  ((uintptr_t) (a) - 0xFFFFFF0000000000)

/// Page map level 4
typedef uint64_t *mm_pml4_t;
/// Page directory pointer table
typedef uint64_t *mm_pdpt_t;
/// Page directory
typedef uint64_t *mm_pagedir_t;
/// Page table
typedef uint64_t *mm_pagetab_t;

/// Virtual memory space
typedef mm_pml4_t mm_space_t;

/// Kernel memory space
extern mm_space_t mm_kernel;

void amd64_mm_init(struct amd64_loader_data *data);
