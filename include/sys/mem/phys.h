/** vim:ft=c.doxygen
 * @file sys/mem/phys.h
 * @brief Physical memory control functions
 */
#pragma once
#include "sys/types.h"

/**
 * @brief Allocate a single physical memory region of MM_PAGE_SIZE bytes
 * @return MM_NADDR on failure, a page-aligned physical address otherwise
 */
uintptr_t mm_phys_alloc_page(void);

/**
 * @brief Allocates a contiguous physical memory region of MM_PAGE_SIZE * \p count bytes
 * @param count Size of the region in pages
 * @return MM_NADDR on failure, a page-aligned physical address otherwise
 */
uintptr_t mm_phys_alloc_contiguous(size_t count);

/**
 * @brief Free a physical page
 * @param addr Physical memory page address, aligned to page boundary
 */
void mm_phys_free_page(uintptr_t addr);

