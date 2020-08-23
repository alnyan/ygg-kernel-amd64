/** vim: set ft=cpp.doxygen :
 * @file sys/vmalloc.h
 * @brief Contiguous virtual memory range management functions
 */
#pragma once
#include "sys/mm.h"

#define VM_ALLOC_WRITE      (MM_PAGE_WRITE)
#define VM_ALLOC_USER       (MM_PAGE_USER)

uintptr_t vmfind(const mm_space_t pd, uintptr_t from, uintptr_t to, size_t npages);
//uintptr_t vmalloc(mm_space_t pd, uintptr_t from, uintptr_t to, size_t npages, uint64_t flags, int usage);
void vmfree(mm_space_t pd, uintptr_t addr, size_t npages);
