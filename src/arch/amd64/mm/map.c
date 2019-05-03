#include "sys/mm.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/mem.h"
#include "pool.h"

static int amd64_mm_map_single(mm_space_t pml4, uintptr_t virt_addr, uintptr_t phys, uint32_t flags) {
    size_t pml4i = (virt_addr >> 39) & 0x1FF;
    size_t pdpti = (virt_addr >> 30) & 0x1FF;
    size_t pdi = (virt_addr >> 21) & 0x1FF;
    size_t pti = (virt_addr >> 12) & 0x1FF;

    mm_pdpt_t pdpt;
    mm_pagedir_t pd;
    mm_pagetab_t pt;

    if (!(pml4[pml4i] & 1)) {
        // Allocate PDPT
        pdpt = (mm_pdpt_t) amd64_mm_pool_alloc();
        if (!pdpt) panic("PDPT alloc failed\n");
        kdebug("Allocated PDPT = %p\n", pdpt);

        pml4[pml4i] = MM_PHYS(pdpt) | 1;
    } else {
        pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & ~0xFFF);
    }

    if (!(pdpt[pdpti] & 1)) {
        // Allocate PD
        pd = (mm_pagedir_t) amd64_mm_pool_alloc();
        if (!pd) panic("PD alloc failed\n");
        kdebug("Allocated PD = %p\n", pd);

        pdpt[pdpti] = MM_PHYS(pd) | 1;
    } else {
        pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & ~0xFFF);
    }

    if (!(pd[pdi] & 1)) {
        // Allocate PT
        pt = (mm_pagetab_t) amd64_mm_pool_alloc();
        if (!pt) panic("PT alloc failed\n");
        kdebug("Allocated PT = %p\n", pt);

        pd[pdi] = MM_PHYS(pt) | 1;
    } else {
        pt = (mm_pagetab_t) MM_VIRTUALIZE(pd[pdi] & ~0xFFF);
    }

    if (pt[pti] & 1) {
        panic("map: entry already present\n");
    }

    pt[pti] = (phys & ~0xFFF) | flags | 1;
    asm volatile("invlpg (%0)"::"a"(virt_addr):"memory");

    return 0;
}

int mm_map_pages_contiguous(mm_space_t pml4, uintptr_t virt_base, uintptr_t phys_base, size_t count, uint32_t flags) {
    for (size_t i = 0; i < count; ++i) {
        uintptr_t virt_addr = virt_base + (i << 12);

        if (amd64_mm_map_single(pml4, virt_addr, phys_base + (i << 12), flags) != 0) {
            return -1;
        }
    }

    return 0;
}
