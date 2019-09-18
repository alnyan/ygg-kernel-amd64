#pragma once
#include "arch/amd64/loader/multiboot.h"
//#include <sys/types.h>
#include <stdint.h>
typedef uintptr_t size_t;

void amd64_phys_memory_map(const multiboot_memory_map_t *mmap, size_t length);

uintptr_t amd64_phys_alloc_contiguous(size_t count);
