#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint16_t limit_lo;
    uint16_t base_lo;
    uint8_t base_mi;
    uint8_t access;
    uint8_t flags;
    uint8_t base_hi;
} amd64_gdt_entry_t;

typedef struct {
    uint16_t size;
    uintptr_t offset;
} __attribute__((packed)) amd64_gdt_ptr_t;

void amd64_gdt_init(void);
