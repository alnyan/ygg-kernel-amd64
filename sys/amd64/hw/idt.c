#include "sys/amd64/hw/idt.h"
#include "sys/amd64/hw/irq.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/types.h"

#define IDT_ENTRY_COUNT     256

extern uintptr_t amd64_exception_vectors[32];
extern void amd64_idt_load(struct amd64_idtr *ptr);

static struct amd64_idt_entry {
    uint16_t base_lo;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_hi;
    uint32_t base_ex;
    uint32_t zero1;
} __attribute__((packed)) idt[IDT_ENTRY_COUNT * AMD64_MAX_SMP] __attribute__((aligned(0x10)));

struct amd64_idtr amd64_idtr[AMD64_MAX_SMP];

void amd64_idt_set(int cpu, int idx, uintptr_t base, uint16_t selector, uint8_t flags) {
    idt[cpu * IDT_ENTRY_COUNT + idx].base_lo = base & 0xFFFF;
    idt[cpu * IDT_ENTRY_COUNT + idx].base_hi = (base >> 16) & 0xFFFF;
    idt[cpu * IDT_ENTRY_COUNT + idx].base_ex = (base >> 32) & 0xFFFFFFFF;
    idt[cpu * IDT_ENTRY_COUNT + idx].selector = selector;
    idt[cpu * IDT_ENTRY_COUNT + idx].flags = flags;
    idt[cpu * IDT_ENTRY_COUNT + idx].zero = 0;
}

void amd64_idt_init(int cpu) {
    kdebug("Setting up IDT for cpu%d\n", cpu);
    memset(&idt[cpu * IDT_ENTRY_COUNT], 0, sizeof(struct amd64_idt_entry) * IDT_ENTRY_COUNT);

    // Exception vectors
    for (size_t i = 0; i < 32; ++i) {
        amd64_idt_set(cpu, i, amd64_exception_vectors[i], 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    }

    irq_init(cpu);

    amd64_idtr[cpu].offset = (uintptr_t) &idt[cpu * IDT_ENTRY_COUNT];
    amd64_idtr[cpu].size = sizeof(struct amd64_idt_entry) * IDT_ENTRY_COUNT - 1;

    // NOTE: This code assumes it runs on the CPU == cpu
    amd64_idt_load(&amd64_idtr[cpu]);
}
