/** vim: set ft=cpp.doxygen :
 * @file sys/vmalloc.h
 * @brief Contiguous virtual memory range management functions
 */
#pragma once
#include "sys/mm.h"

#define VM_ALLOC_USER   (1 << 0)
#define VM_ALLOC_WRITE  (1 << 1)

/**
 * @brief Find a free contiguous memory range inside a given one
 *        in a virtual memory space.
 * @param pd Virtual memory space
 * @param from Start of the range to allocate from
 * @param to End of the range to allocate from
 * @param npages Size of the needed range in 4K pages
 * @return Address of the resulting range on success,
 *         MM_NADDR if such range cannot be allocated
 */
uintptr_t vmfind(const mm_space_t pd, uintptr_t from, uintptr_t to, size_t npages);

/**
 * @brief Allocate a contiguous virtual memory range in a space
 *        within `from'..`to' limits and map it to a set of
 *        physical pages (which may not be contiguous).
 * @param pd Virtual memory space
 * @param from Start of the range to allocate from
 * @param to End of the range to allocate from
 * @param npages Count of 4K pages to allocate
 * @param flags:
 *        * VM_ALLOC_USER - The range is available for userspace
 *        * VM_ALLOC_WRITE - The range is writable
 * @return Virtual address of the resulting range on success,
 *         MM_NADDR otherwise
 */
uintptr_t vmalloc(mm_space_t pd, uintptr_t from, uintptr_t to, size_t npages, int flags);

/**
 * @brief Deallocate a virtual memory range and physical pages
 *        it's mapped to.
 * @param pd Virtual memory space
 * @param addr Address of the beginning of the range
 * @param npages Size of the range in 4K pages
 */
void vmfree(mm_space_t pd, uintptr_t addr, size_t npages);
