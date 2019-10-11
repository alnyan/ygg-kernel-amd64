#include "sys/amd64/hw/gdt.h"
#define GDT_SIZE    7

extern void amd64_gdt_load(void *p);

static amd64_gdt_entry_t gdt[GDT_SIZE * AMD64_MAX_SMP] = { 0 };
static amd64_gdt_ptr_t amd64_gdtr[AMD64_MAX_SMP];
static amd64_tss_t amd64_tss[AMD64_MAX_SMP] = { 0 };

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

static void amd64_gdt_set(int cpu, int idx, uint32_t base, uint32_t limit, uint8_t flags, uint8_t access) {
    gdt[idx + cpu * GDT_SIZE].base_lo = base & 0xFFFF;
    gdt[idx + cpu * GDT_SIZE].base_mi = (base >> 16) & 0xFF;
    gdt[idx + cpu * GDT_SIZE].base_hi = (base >> 24) & 0xFF;
    gdt[idx + cpu * GDT_SIZE].access = access;
    gdt[idx + cpu * GDT_SIZE].flags = (flags & 0xF0) | ((limit >> 16) & 0xF);
    gdt[idx + cpu * GDT_SIZE].limit_lo = limit & 0xFFFF;
}

amd64_gdt_ptr_t *amd64_gdtr_get(int cpu) {
    return &amd64_gdtr[cpu];
}

amd64_tss_t *amd64_tss_get(int cpu) {
    return &amd64_tss[cpu];
}

void amd64_gdt_init(void) {
    for (size_t i = 0; i < AMD64_MAX_SMP; ++i) {
        amd64_gdt_set(i, 0, 0, 0, 0, 0);
        amd64_gdt_set(i, 1, 0, 0,
                      GDT_FLG_LONG,
                      GDT_ACC_PR | GDT_ACC_S | GDT_ACC_EX);
        amd64_gdt_set(i, 2, 0, 0,
                      0,
                      GDT_ACC_PR | GDT_ACC_S | GDT_ACC_RW);
        amd64_gdt_set(i, 3, 0, 0,
                      0,
                      GDT_ACC_PR | GDT_ACC_R3 | GDT_ACC_S | GDT_ACC_RW);
        amd64_gdt_set(i, 4, 0, 0,
                      GDT_FLG_LONG,
                      GDT_ACC_PR | GDT_ACC_R3 | GDT_ACC_S | GDT_ACC_EX);
        amd64_gdt_set(i, 5, ((uintptr_t) &amd64_tss[i]) & 0xFFFFFFFF, sizeof(amd64_tss_t) - 1,
                      GDT_FLG_LONG,
                      GDT_ACC_PR | GDT_ACC_AC | GDT_ACC_EX);
        *(uint64_t *) &gdt[i * GDT_SIZE + 6] = ((uintptr_t) &amd64_tss[i]) >> 32;

        amd64_gdtr[i].size = GDT_SIZE * sizeof(amd64_gdt_entry_t) - 1;
        amd64_gdtr[i].offset = (uintptr_t) &gdt[GDT_SIZE * i];
    }

    amd64_gdt_load(&amd64_gdtr);
}
