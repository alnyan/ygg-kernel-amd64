/* vim: set ft=cpp.doxygen : */
#pragma once
#include <stdint.h>

/// The place where the kernel pages are virtually mapped to
#define KERNEL_VIRT_BASE                    0xFFFFFF0000000000

/// amd64 standard states that addresses' upper bits are copies of the 47th bit, so these need to be
///  stripped down to only 48 bits
#define AMD64_MM_STRIPSX(a)                 ((uintptr_t) (a) & 0xFFFFFFFFFFFF)
#define AMD64_MM_ADDRSX(a)                  (((uintptr_t) (a) & (1ULL << 47)) ? \
                                                (0xFFFFFF0000000000 | ((uintptr_t) (a))) : \
                                                ((uintptr_t) (a)))

#define MM_VIRTUALIZE(a)                    ((uintptr_t) (a) + 0xFFFFFF0000000000)
#define MM_PHYS(a)                          ((uintptr_t) (a) - 0xFFFFFF0000000000)

#define MM_PAGE_SIZE                        0x1000

#define MM_PTE_INDEX_MASK                   0x1FF
#define MM_PTE_COUNT                        512
#define MM_PTE_FLAGS_MASK                   (0xFFF | MM_PAGE_NOEXEC)
#define MM_PTE_MASK                         (~MM_PTE_FLAGS_MASK)

#define MM_PAGE_MASK                        (~0xFFF)

// Same as L1 mask
#define MM_PAGE_OFFSET_MASK                 0xFFF
#define MM_PAGE_L2_OFFSET_MASK              ((1 << MM_PDI_SHIFT) - 1)
#define MM_PAGE_L3_OFFSET_MASK              ((1 << MM_PDPTI_SHIFT) - 1)
#define MM_PAGE_L4_OFFSET_MASK              ((1 << MM_PML4I_SHIFT) - 1)

#define MM_PTI_SHIFT                        12
#define MM_PDI_SHIFT                        21
#define MM_PDPTI_SHIFT                      30
#define MM_PML4I_SHIFT                      39

#define MM_PAGE_PRESENT                     (1ULL << 0)
#define MM_PAGE_WRITE                       (1ULL << 1)
#define MM_PAGE_USER                        (1ULL << 2)
#define MM_PAGE_WT                          (1ULL << 3)
#define MM_PAGE_NOCACHE                     (1ULL << 4)
#define MM_PAGE_ACCESSED                    (1ULL << 5)
#define MM_PAGE_DIRTY                       (1ULL << 6)
#define MM_PAGE_HUGE                        (1ULL << 7)
#define MM_PAGE_GLOBAL                      (1ULL << 8)
#define MM_PAGE_NOEXEC                      (1ULL << 63)

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

void amd64_mm_init(void);
