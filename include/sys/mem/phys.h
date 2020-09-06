/** vim:ft=c.doxygen
 * @file sys/mem/phys.h
 * @brief Physical memory control functions
 */
#pragma once
#include "sys/types.h"
#include "sys/list.h"

extern struct page *mm_pages;

#define PHYS2PAGE(phys) \
    (&mm_pages[((uintptr_t) (phys)) / MM_PAGE_SIZE])
#define PAGE2PHYS(page) \
    (MM_PAGE_SIZE * ((uintptr_t) (phys) - (uintptr_t) pages) / sizeof(struct page))

#define PG_ALLOC                (1 << 0)
#define PG_MMAPED               (1 << 1)

struct page {
    uint64_t flags;
    enum page_usage {
        PU_UNKNOWN = 0,
        PU_PRIVATE,             // Program data: image + mmap()ed anonymous regions
        PU_SHARED,              // Shared memory mapping
        PU_DEVICE,              // Possibly shared device mapping
        PU_KERNEL,              // Not a userspace page
        PU_PAGING,              // Paging structures
        PU_CACHE,
        _PU_COUNT
    } usage;
    size_t refcount;
};

struct mm_phys_reserved {
    uintptr_t begin, end;
    struct list_head link;
};

struct mm_phys_stat {
    size_t pages_total;
    size_t pages_free;
    size_t pages_used_kernel;
    size_t pages_used_user;
    size_t pages_used_shared;
    size_t pages_used_paging;
    size_t pages_used_cache;
};

void mm_phys_reserve(const char *use, struct mm_phys_reserved *res);
void mm_phys_stat(struct mm_phys_stat *st);

/**
 * @brief Allocate a single physical memory region of MM_PAGE_SIZE bytes
 * @return MM_NADDR on failure, a page-aligned physical address otherwise
 */
uintptr_t mm_phys_alloc_page(enum page_usage pu);

/**
 * @brief Allocates a contiguous physical memory region of MM_PAGE_SIZE * \p count bytes
 * @param count Size of the region in pages
 * @return MM_NADDR on failure, a page-aligned physical address otherwise
 */
uintptr_t mm_phys_alloc_contiguous(size_t count, enum page_usage pu);

/**
 * @brief Free a physical page
 * @param addr Physical memory page address, aligned to page boundary
 */
void mm_phys_free_page(uintptr_t addr);

