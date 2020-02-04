#pragma once
#include <stdint.h>

#define KERNEL_CMDLINE_MAX          256

struct amd64_loader_data {
    uint32_t multiboot_info_ptr;
    uint32_t symtab_ptr;
    uint32_t symtab_size;
    uint32_t strtab_ptr;
    uint32_t strtab_size;
    uint32_t initrd_ptr;
    uint32_t initrd_len;
    char cmdline[KERNEL_CMDLINE_MAX];
    // Just in case
    uint8_t checksum;
} __attribute__((packed));
