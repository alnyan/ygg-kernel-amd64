/** vim: set ft=cpp.doxygen :
 * @file arch/amd64/mm/map.h
 * @brief amd64-specific virtual memory space functions
 */
#pragma once
#include "sys/types.h"
#include "sys/mm.h"

/// Splits userspace mappings and kernelspace ones
#define AMD64_PML4I_USER_END    255

/**
 * @brief Translate a virtual address in `space' to its physical mapping (if one exists).
 *        May optionally return access flags in `flags' if non-NULL
 * @param space Virtual memory space
 * @param vaddr Virtual address
 * @param flags Access flags storage pointer or NULL
 * @return Physical address corresponding to `vaddr' in `space' if mapping exists,
 *         MM_NADDR otherwise.
 */
uintptr_t amd64_map_get(const mm_space_t space, uintptr_t vaddr, uint64_t *flags);

/**
 * @brief Remove a virtual address mapping from memory space, optionally checking if it
 *        is of a specific page size.
 * @param space Virtual memory space
 * @param vaddr Virtual address
 * @param size Expected page size:
 *             * 1 for 4KiB
 *             * 2 for 2MiB
 *             * 3 for 1GiB
 *             * 0 for any size
 * @return Physical address of the unmapped page (in case it has to be freed),
 *         MM_NADDR in case the mapping does not exist or the size does not match.
 */
uintptr_t amd64_map_umap(mm_space_t space, uintptr_t vaddr, uint32_t size);

/**
 * @brief Add a single page mapping to `pml4'
 * @param pml4 Virtual memory space
 * @param virt_addr Source virtual address
 * @param phys Destination physical address
 * @param flags Access flags:
 *              * TODO: size-specific
 *              * AMD64_MAP_WRITE
 *              * AMD64_MAP_USER
 * @return 0 on success, -1 otherwise
 */
int amd64_map_single(mm_space_t pml4, uintptr_t virt_addr, uintptr_t phys, uint64_t flags);
