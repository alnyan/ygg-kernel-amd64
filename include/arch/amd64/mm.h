#pragma once
#include <stdint.h>

/// The place where the kernel pages are virtually mapped to
#define KERNEL_VIRT_BASE        0xFFFFFF0000000000

/// amd64 standard states that addresses' upper bits are copies of the 47th bit, so these need to be
///  stripped down to only 48 bits
#define AMD64_MM_MASK(a)        ((uintptr_t) (a) & 0xFFFFFFFFFFFF)

#define MM_VIRTUALIZE(a)        ((uintptr_t) (a) + 0xFFFFFF0000000000)
#define MM_PHYS(a)              ((uintptr_t) (a) - 0xFFFFFF0000000000)

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

void amd64_mm_init(void);
