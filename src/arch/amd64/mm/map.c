#include "sys/mm.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/mem.h"
#include "arch/amd64/mm/pool.h"

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

#if defined(KERNEL_TEST_MODE)
    kdebug("map %p -> %p\n", virt_addr, phys);
#endif
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

void mm_describe(const mm_space_t pml4) {
    kdebug("Memory space V:%p:\n", pml4);

    mm_pdpt_t pdpt;
    mm_pagedir_t pd;
    mm_pagetab_t pt;

    for (size_t pml4i = 0; pml4i < 512; ++pml4i) {
        if (!(pml4[pml4i] & 1)) {
            continue;
        }

        if (pml4[pml4i] & (1 << 7)) {
            kdebug("`- PHYS %p\n", pml4[pml4i] & ~0xFFF);
            continue;
        }

        pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & ~0xFFF);
        kdebug("`- PDPT %p\n", pdpt);

        for (size_t pdpti = 0; pdpti < 512; ++pdpti) {
            if (!(pdpt[pdpti] & 1)) {
                continue;
            }

            if (pdpt[pdpti] & (1 << 7)) {
                if (pml4i < 510) {
                    kdebug("`- PHYS %p\n", pdpt[pdpti] & ~0xFFF);
                }
                continue;
            }

            pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & ~0xFFF);
            kdebug(" `- PDIR %p\n", pd);

            for (size_t pdi = 0; pdi < 512; ++pdi) {
                if (!(pd[pdi] & 1)) {
                    continue;
                }

                if (pd[pdi] & (1 << 7)) {
                    kdebug("`- PHYS %p\n", pdpt[pdpti] & ~0xFFF);
                    continue;
                }

                pt = (mm_pagetab_t) MM_VIRTUALIZE(pd[pdi] & ~0xFFF);
                kdebug("  `- PTAB %p\n", pt);

                for (size_t pti = 0; pti < 512; ++pti) {
                    if (!(pt[pti] & 1)) {
                        continue;
                    }

                    kdebug("   `- PHYS %p\n", pt[pti] & ~0xFFF);
                }
            }
        }
    }
}
