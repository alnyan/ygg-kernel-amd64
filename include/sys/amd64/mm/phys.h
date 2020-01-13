#pragma once
#include "sys/amd64/loader/multiboot.h"
#include "sys/types.h"
#include <stdint.h>

struct amd64_phys_stat {
    size_t pages_free;
    size_t pages_used;
    size_t limit;
};

void amd64_phys_memory_map(const multiboot_memory_map_t *mmap, size_t length);

void amd64_phys_free(uintptr_t page);
uintptr_t amd64_phys_alloc_page(void);
uintptr_t amd64_phys_alloc_contiguous(size_t count);

void amd64_phys_stat(struct amd64_phys_stat *st);
