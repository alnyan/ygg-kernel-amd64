#pragma once
#include <stdint.h>

struct amd64_loader_data {
    uint32_t multiboot_info_ptr;
    uint32_t initrd_ptr;
    uint32_t initrd_len;
    // Just in case
    uint8_t checksum;
} __attribute__((packed));
