#pragma once
#include "arch/amd64/multiboot2.h"
#include "sys/types.h"
#include <stdint.h>

#define MM_PHYS_MMAP_FMT_MULTIBOOT2 0
#define MM_PHYS_MMAP_FMT_YBOOT      1

struct mm_phys_memory_map {
    int    format;
    void  *address;
    size_t entry_count;
    size_t entry_size;
};

//void amd64_phys_memory_map(const struct multiboot_tag_mmap *mmap);
void amd64_phys_memory_map(const struct mm_phys_memory_map *mmap);
