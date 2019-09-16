#include "arch/amd64/hw/gdt.h"

extern void amd64_reload_segs(void);

#define AMD64_GDT_SIZE  3

#define GDT_ACC_AC      (1 << 0)
#define GDT_ACC_RW      (1 << 1)
#define GDT_ACC_DC      (1 << 2)
#define GDT_ACC_EX      (1 << 3)
#define GDT_ACC_S       (1 << 4)
#define GDT_ACC_R0      (0 << 5)
#define GDT_ACC_R1      (1 << 5)
#define GDT_ACC_R2      (2 << 5)
#define GDT_ACC_R3      (3 << 5)
#define GDT_ACC_PR      (1 << 7)

#define GDT_FLG_LONG    (1 << 5)
#define GDT_FLG_SZ      (1 << 6)
#define GDT_FLG_GR      (1 << 7)

static amd64_gdt_entry_t gdt[AMD64_GDT_SIZE];
static amd64_gdt_ptr_t gdtr;

static void amd64_gdt_set(int idx, uint32_t base, uint32_t limit, uint8_t flags, uint8_t access) {
    gdt[idx].base_lo = base & 0xFFFF;
    gdt[idx].base_mi = (base >> 16) & 0xFF;
    gdt[idx].base_hi = (base >> 24) & 0xFF;
    gdt[idx].access = access;
    gdt[idx].flags = (flags & 0xF0) | ((limit >> 16) & 0xF);
    gdt[idx].limit_lo = limit & 0xFFFF;
}

void amd64_gdt_init(void) {
    gdtr.size = sizeof(gdt) - 1;
    gdtr.offset = (uintptr_t) gdt;

    amd64_gdt_set(0, 0, 0, 0, 0);
    amd64_gdt_set(1, 0, 0, GDT_FLG_LONG, GDT_ACC_PR | GDT_ACC_R0 | GDT_ACC_EX | GDT_ACC_S | GDT_ACC_RW);
    amd64_gdt_set(2, 0, 0, 0, GDT_ACC_PR | GDT_ACC_R0 | GDT_ACC_S | GDT_ACC_RW);

    asm volatile ("lea gdtr(%rip), %rax; lgdt (%rax)");
    amd64_reload_segs();
}
