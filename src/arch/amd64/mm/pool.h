#pragma once
#include <stddef.h>
#include <stdint.h>

/// Initialize the paging structure allocation pool
void amd64_mm_pool_init(uintptr_t base, size_t size);

/// Allocate a paging structure
uint64_t *amd64_mm_pool_alloc(void);

/// Free a paging structure
void amd64_mm_pool_free(uint64_t *obj);
