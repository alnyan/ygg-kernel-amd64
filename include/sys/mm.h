/** vim: set ft=cpp.doxygen :
 * @file sys/mm.h
 * @brief Virtual memory space management functions
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(ARCH_AMD64)
#include "arch/amd64/mm/mm.h"
#endif

struct thread;

/// An invalid address analogous to NULL
#define MM_NADDR                ((uintptr_t) -1)

#define MM_CLONE_FLG_KERNEL     (1 << 0)
#define MM_CLONE_FLG_USER       (1 << 1)

#define userspace

mm_space_t mm_space_create(void);
int mm_space_clone(mm_space_t dst, const mm_space_t src, uint32_t flags);
int mm_space_fork(struct thread *dst, const struct thread *src, uint32_t flags);
void mm_space_release(struct thread *thr);
void mm_space_free(struct thread *thr);

void mm_describe(const mm_space_t pd);

int mm_map_single(mm_space_t pd, uintptr_t virt_page, uintptr_t phys_page, uint64_t flags, int usage);
uintptr_t mm_umap_single(mm_space_t pd, uintptr_t virt_page, uint32_t size);
uintptr_t mm_map_get(mm_space_t pd, uintptr_t virt, uint64_t *rflags);

void userptr_check(const void *ptr);
