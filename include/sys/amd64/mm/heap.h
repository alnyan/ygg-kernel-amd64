// vim: set ft=cpp.doxygen :
#pragma once
#include "sys/heap.h"
#include <stdint.h>

#define KERNEL_HEAP     (16 * 1024 * 1024)

/**
 * @brief Initialize heap instance of size `sz' at `phys_base'
 * @param heap Heap instance
 * @param phys_base Physical address of the first byte of the heap
 * @param sz Count of bytes available for heap usage
 * @note Physical addresses are used internally for heap because this
 *       allows heap to just MM_VIRTUALIZE allocated pointers no matter
 *       where the kernel heap actually resides, which could be the
 *       problem if we created a separate mapping for kernel heap
 *       memory.
 */
void amd64_heap_init(heap_t *heap, uintptr_t phys_base, size_t sz);

// Debug info
/**
 * @return Number of blocks in heap
 */
size_t amd64_heap_blocks(const heap_t *heap);

/**
 * @brief Print kernel heap block list to debug output
 */
void amd64_heap_dump(const heap_t *heap);
