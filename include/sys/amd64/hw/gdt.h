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
    uint32_t __res0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t __res1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t __res2;
    uint16_t __res3;
    uint16_t iopb_base;
}__attribute__((packed)) amd64_tss_t;

typedef struct {
    uint16_t size;
    uintptr_t offset;
} __attribute__((packed)) amd64_gdt_ptr_t;

amd64_gdt_ptr_t *amd64_gdtr_get(int cpu);
amd64_tss_t *amd64_tss_get(int cpu);

void amd64_gdt_init(void);
