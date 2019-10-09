#include "sys/amd64/hw/idt.h"
#include "sys/string.h"
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

extern uintptr_t amd64_exception_vectors[32];
extern void amd64_irq0(void);

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

static void amd64_idt_set(int idx, uintptr_t base, uint16_t selector, uint8_t flags) {
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
    amd64_idt_set(32, (uintptr_t) amd64_irq0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    asm volatile ("lidt amd64_idtr(%rip)");
}
