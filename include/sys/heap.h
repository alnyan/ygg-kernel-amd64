// vim: set ft=cpp.doxygen :
// Platform-generic kernel heap header
#pragma once
#include <sys/types.h>

#define kmalloc(sz)     heap_alloc(heap_global, sz)
#define kfree(p)        heap_free(heap_global, p)

/// Opaque type for kernel heap usage
typedef struct kernel_heap heap_t;

/// Implementations provide at least global heap
extern heap_t *heap_global;

/**
 * @brief Allocate `count' bytes
 * @param count Number of bytes to allocate
 */
void *heap_alloc(heap_t *heap, size_t count);

/**
 * @brief Make memory block at `ptr' usable again
 * @warning ptr must be a valid pointer allocated from the heap, otherwise
 *          kernel panic is raised
 */
void heap_free(heap_t *heap, void *ptr);

/**
 * @brief Estimate heap free memory size
 * @return Approximate number of usable bytes
 */
size_t heap_info_free(heap_t *heap);

#if defined(ARCH_AMD64)
#include "arch/amd64/mm/heap.h"
#endif
