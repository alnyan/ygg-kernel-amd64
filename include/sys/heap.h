/** vim: set ft=cpp.doxygen :
 * @file sys/heap.h
 * @brief Kernel heap memory managament functions
 */
#pragma once
#include "sys/types.h"

/**
 * @brief Common kernel heap alloc
 * @see heap_alloc
 */
#define kmalloc(sz)     heap_alloc(heap_global, sz)

/**
 * @brief Common kernel heap free
 * @see heal_free
 */
#define kfree(p)        heap_free(heap_global, p)

/// Opaque type for kernel heap usage
typedef struct kernel_heap heap_t;

/// Implementations provide at least global heap
extern heap_t *heap_global;

/**
 * @brief Allocate `count' bytes
 * @param heap Heap instance
 * @param count Number of bytes to allocate
 * @return Virtual address of the block on success,
 *         NULL otherwise
 */
void *heap_alloc(heap_t *heap, size_t count);

/**
 * @brief Make memory block at `ptr' usable again if `ptr' is non-NULL
 * @param heap Heap instance
 * @param ptr Pointer to a heap block or NULL
 * @warning ptr must be a valid pointer allocated from the heap, otherwise
 *          kernel panic is raised
 */
void heap_free(heap_t *heap, void *ptr);

/**
 * @brief Estimate heap free memory size
 * @param heap Heap instance
 * @return Approximate number of usable bytes
 */
size_t heap_info_free(heap_t *heap);

#if defined(ARCH_AMD64)
#include "sys/amd64/mm/heap.h"
#endif
