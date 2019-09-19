#pragma once
#include "arch/amd64/loader/multiboot.h"
#include "sys/types.h"
#include <stdint.h>

void amd64_phys_memory_map(const multiboot_memory_map_t *mmap, size_t length);

void amd64_phys_free(uintptr_t page);
uintptr_t amd64_phys_alloc_page(void);
uintptr_t amd64_phys_alloc_contiguous(size_t count);
