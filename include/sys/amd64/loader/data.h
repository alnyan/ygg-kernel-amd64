#pragma once
#include <stdint.h>

struct amd64_loader_data {
    uint32_t multiboot_info_ptr;
    // Just in case
    uint8_t checksum;
} __attribute__((packed));
