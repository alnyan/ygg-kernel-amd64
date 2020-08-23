#pragma once
#include "arch/amd64/multiboot2.h"
#include "sys/types.h"
#include <stdint.h>

void amd64_phys_memory_map(const struct multiboot_tag_mmap *mmap);
