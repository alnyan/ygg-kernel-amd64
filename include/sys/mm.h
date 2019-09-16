/* vim: set ft=cpp.doxygen : */
#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(ARCH_AMD64)
#include "arch/amd64/mm.h"
#endif

/// An invalid address analogous to NULL
#define MM_NADDR    ((uintptr_t) -1)

#define userspace

/**
 * @brief Creates a virtual memory space with underlying data structures specific to current
 *  platform
 */
mm_space_t mm_space_create(void);

/**
 * @brief Copies the mappings from one memory space to another preserving the original physical
 *  pages
 * @param dst - Destination space
 * @param src - Source space
 * @param flags - Flags:
 *  * MM_CLONE_FLG_KERNEL - clone kernel-region mappings
 *  * MM_CLONE_FLG_USER - clone userspace mappings
 * @return 0 on success
 * @return Non-zero values in case of error
 */
int mm_space_clone(mm_space_t dst, const mm_space_t src, uint32_t flags);

/**
 * @brief Performs a "fork" of a memory space: copies the original mappings to the destination space
 *  and optionally replaces userspace mappings with cloned physical pages
 * @param dst - Destination space
 * @param src - Source space
 * @param flags - Flags:
 *  * MM_CLONE_FLG_KERNEL - clone kernel-region mappings
 *  * MM_CLONE_FLG_USER - clone userspace mappings and underlying physical pages
 * @return 0 on success
 * @return Non-zero values in case of error
 */
int mm_space_fork(mm_space_t dst, const mm_space_t src, uint32_t flags);

/**
 * @brief Destroy the virtual memory space and free used resources
 * @param pd - Virtual memory space
 */
void mm_space_free(mm_space_t pd);

/**
 * @brief Dump debug information about the memory space
 * @param pd - Virtual memory space
 */
void mm_describe(const mm_space_t pd);


/**
 * @brief Map a contiguous physical memory region to a contiguous virtual memory region
 * @param pd - Memory space
 * @param virt_base - Beginning of the virtual memory range
 * @param phys_base - Beginning of the physical memory range
 * @param count - Count of pages to be mapped
 * @param flags - Flags:
 *  * MM_FLG_WR - Writable
 *  * MM_FLG_US - Available for userspace
 * @return 0 on success
 * @return Non-zero values in case of error
 */
int mm_map_pages_contiguous(mm_space_t pd, uintptr_t virt_base, uintptr_t phys_base, size_t count, uint32_t flags);

/**
 * @brief Map a set of physical pages to a virtual memory region
 * @param pd - Memory space
 * @param virt_base - Beginning of the virtual memory range
 * @param pages - Array of physical pages to be mapped
 * @param count - Count of pages to be mapped
 * @param flags - Flags:
 *  * MM_FLG_WR - Writable
 *  * MM_FLG_US - Available for userspace
 * @return 0 on success
 * @return Non-zero values in case of error
 */
int mm_map_range_pages(mm_space_t pd, uintptr_t virt_base, uintptr_t *pages, size_t count, uint32_t flags);

/**
 * @brief Remove virtual memory mappings for the specified region, optionally de-allocating the
 *  underlying physical memory pages
 * @param pd - Memory space
 * @param virt_base - Beginning of the virtual memory range
 * @param count - Count of pages to unmap
 * @param flags - Flags:
 *  * MM_FLG_NOPHYS - Don't de-allocate the physical pages used by the mappings
 * @return 0 on success
 * @return Non-zero values in case of error
 */
int mm_umap_range(mm_space_t pd, uintptr_t virt_base, size_t count, uint32_t flags);

/**
 * @brief Translate a virtual memory address to its physical counterpart
 * @param pd - Memory space
 * @param virt - Virtual address
 * @param rflags - (output, nullable) variable to store the mapping's flags:
 *  * MM_FLG_WR
 *  * MM_FLG_US
 * @return Physical address if the mapping is present
 * @return MM_NADDR if the mapping is not present
 */
uintptr_t mm_translate(mm_space_t pd, uintptr_t virt, uint32_t *rflags);



/**
 * @brief Copy data from kernel memory space to user
 * @param pd - Memory space
 * @param dst - Destination address
 * @param src - Source address
 * @param count - Number of bytes to transfer
 * @return Number of bytes copied on success
 * @return -1 on error
 */
int mm_memcpy_kernel_to_user(mm_space_t pd, userspace void *dst, const void *src, size_t count);

/**
 * @brief Copy data from user memory space to kernel
 * @param pd - Memory space
 * @param dst - Destination address
 * @param src - Source address
 * @param count - Number of bytes to transfer
 * @return Number of bytes copied on success
 * @return -1 on error
 */
int mm_memcpy_user_to_kernel(mm_space_t pd, void *dst, const userspace void *src, size_t count);
