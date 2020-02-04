#pragma once
#include "sys/types.h"

#define IDT_FLG_TASK32      0x5
#define IDT_FLG_INT16       0x6
#define IDT_FLG_TRAP16      0x7
#define IDT_FLG_INT32       0xE
#define IDT_FLG_TRAP32      0xF
#define IDT_FLG_SS          (1 << 4)
#define IDT_FLG_R0          (0 << 5)
#define IDT_FLG_R1          (1 << 5)
#define IDT_FLG_R2          (2 << 5)
#define IDT_FLG_R3          (3 << 5)
#define IDT_FLG_P           (1 << 7)

struct amd64_idtr {
    uint16_t size;
    uintptr_t offset;
} __attribute__((packed));

extern struct amd64_idtr amd64_idtr[AMD64_MAX_SMP];

void amd64_idt_set(int cpu, int idx, uintptr_t base, uint16_t selector, uint8_t flags);

void amd64_idt_init(int cpu);
