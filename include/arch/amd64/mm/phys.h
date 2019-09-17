#pragma once
#include "arch/amd64/loader/multiboot.h"
#include <sys/types.h>

void amd64_phys_memory_map(const multiboot_memory_map_t *mmap, size_t length);
