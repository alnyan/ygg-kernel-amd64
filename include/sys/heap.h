/** vim: set ft=cpp.doxygen :
 * @file sys/heap.h
 * @brief Kernel heap memory managament functions
 */
#pragma once
#include "sys/types.h"

#define HEAP_TRACE 1
#define HEAP_TRACE_ALLOC        1
#define HEAP_TRACE_FREE         2

struct heap_stat {
    size_t alloc_count;
    size_t alloc_size;
    size_t free_size;
    size_t total_size;
};

#if defined(HEAP_TRACE)
#define kmalloc(sz)     ({ \
        void *p = heap_alloc(heap_global, sz); \
        heap_trace(HEAP_TRACE_ALLOC, __FILE__, __func__, __LINE__, p, sz); \
        p; \
    })

#define kfree(p)        heap_trace(HEAP_TRACE_FREE, __FILE__, __func__, __LINE__, p, 0); \
                        heap_free(heap_global, p)

void heap_trace(int type, const char *file, const char *func, int line, void *ptr, size_t count);
#else
#define kmalloc(sz)     heap_alloc(heap_global, sz)
#define kfree(p)        heap_free(heap_global, p)
#endif

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

void heap_stat(heap_t *heap, struct heap_stat *st);

#if defined(ARCH_AMD64)
#include "arch/amd64/mm/heap.h"
#endif
