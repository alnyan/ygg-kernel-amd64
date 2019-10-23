#include "sys/amd64/hw/idt.h"
#include "sys/amd64/hw/irq.h"
#include "sys/string.h"
#include "sys/types.h"

extern uintptr_t amd64_exception_vectors[32];

static struct amd64_idt_entry {
    uint16_t base_lo;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_hi;
    uint32_t base_ex;
    uint32_t zero1;
} __attribute__((packed)) idt[256] __attribute__((aligned(0x10)));

const struct amd64_idtr amd64_idtr = {
    .size = sizeof(idt) - 1,
    .offset = (uintptr_t) idt
};

void amd64_idt_set(int idx, uintptr_t base, uint16_t selector, uint8_t flags) {
    idt[idx].base_lo = base & 0xFFFF;
    idt[idx].base_hi = (base >> 16) & 0xFFFF;
    idt[idx].base_ex = (base >> 32) & 0xFFFFFFFF;
    idt[idx].selector = selector;
    idt[idx].flags = flags;
    idt[idx].zero = 0;
}

void amd64_idt_init(void) {
    memset(idt, 0, sizeof(idt));
    // Exception vectors
    for (size_t i = 0; i < 32; ++i) {
        amd64_idt_set(i, amd64_exception_vectors[i], 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    }

    irq_init();

    asm volatile ("lidt amd64_idtr(%rip)");
}
