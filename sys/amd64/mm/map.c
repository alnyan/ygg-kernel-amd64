#include "sys/mm.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/amd64/mm/map.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/pool.h"

uintptr_t amd64_map_get(const mm_space_t pml4, uintptr_t vaddr, uint64_t *flags) {
    vaddr = AMD64_MM_STRIPSX(vaddr);
    size_t pml4i = (vaddr >> 39) & 0x1FF;
    size_t pdpti = (vaddr >> 30) & 0x1FF;
    size_t pdi = (vaddr >> 21) & 0x1FF;
    size_t pti = (vaddr >> 12) & 0x1FF;

    mm_pdpt_t pdpt;
    mm_pagedir_t pd;
    mm_pagetab_t pt;

    if (!(pml4[pml4i] & 1)) {
        return MM_NADDR;
    }

    if (pml4[pml4i] & (1 << 7)) {
        panic("NYI\n");
    }

    pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & ~0xFFF);

    if (!(pdpt[pdpti] & 1)) {
        return MM_NADDR;
    }

    if (pdpt[pdpti] & (1 << 7)) {
        if (flags) {
            *flags = 2;
        }
        return (pdpt[pdpti] & ~0xFFF) | (vaddr & ((1 << 30) - 1));
    }

    pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & ~0xFFF);

    if (!(pd[pdi] & 1)) {
        return MM_NADDR;
    }

    if (pd[pdi] & (1 << 7)) {
        if (flags) {
            *flags = 1;
        }
        return (pd[pti] & ~0xFFF) | (vaddr & ((1 << 21) - 1));
    }

    pt = (mm_pagetab_t) MM_VIRTUALIZE(pd[pdi] & ~0xFFF);

    if (!(pt[pti] & 1)) {
        return MM_NADDR;
    }

    if (flags) {
        *flags = 0;
    }

    return (pt[pti] & ~0xFFF) | (vaddr & 0xFFF);
}

uintptr_t amd64_map_umap(mm_space_t pml4, uintptr_t vaddr, uint32_t size) {
    vaddr = AMD64_MM_STRIPSX(vaddr);
    // TODO: support page sizes other than 4KiB
    // (Though I can't think of any reason to use it)
    size_t pml4i = (vaddr >> 39) & 0x1FF;
    size_t pdpti = (vaddr >> 30) & 0x1FF;
    size_t pdi = (vaddr >> 21) & 0x1FF;
    size_t pti = (vaddr >> 12) & 0x1FF;

    mm_pdpt_t pdpt;
    mm_pagedir_t pd;
    mm_pagetab_t pt;

    if (!(pml4[pml4i] & 1)) {
        return MM_NADDR;
    }

    if (pml4[pml4i] & (1 << 7)) {
        panic("NYI\n");
    }

    pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & ~0xFFF);

    if (!(pdpt[pdpti] & 1)) {
        return MM_NADDR;
    }

    if (pdpt[pdpti] & (1 << 7)) {
        panic("NYI\n");
    }

    pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & ~0xFFF);

    if (!(pd[pdi] & 1)) {
        return MM_NADDR;
    }

    if (pd[pdi] & (1 << 7)) {
        panic("NYI\n");
    }

    pt = (mm_pagetab_t) MM_VIRTUALIZE(pd[pdi] & ~0xFFF);

    if (!(pt[pti] & 1)) {
        return MM_NADDR;
    }

    uint64_t old = pt[pti] & ~0xFFF;
    pt[pti] = 0;
    asm volatile("invlpg (%0)"::"a"(vaddr):"memory");
    return old;
}

int amd64_map_single(mm_space_t pml4, uintptr_t virt_addr, uintptr_t phys, uint32_t flags) {
    virt_addr = AMD64_MM_STRIPSX(virt_addr);
    // TODO: support page sizes other than 4KiB
    // (Though I can't think of any reason to use it)
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
        assert(pdpt, "PDPT alloc failed\n");
        kdebug("Allocated PDPT = %p\n", pdpt);

        pml4[pml4i] = MM_PHYS(pdpt) | (1 << 2) | (1 << 1) | 1;
    } else {
        pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & ~0xFFF);
    }

    if (!(pdpt[pdpti] & 1)) {
        // Allocate PD
        pd = (mm_pagedir_t) amd64_mm_pool_alloc();
        assert(pd, "PD alloc failed\n");
        kdebug("Allocated PD = %p\n", pd);

        pdpt[pdpti] = MM_PHYS(pd) | (1 << 2) | (1 << 1) | 1;
    } else {
        pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & ~0xFFF);
    }

    if (!(pd[pdi] & 1)) {
        // Allocate PT
        pt = (mm_pagetab_t) amd64_mm_pool_alloc();
        assert(pt, "PT alloc failed\n");
        kdebug("Allocated PT = %p\n", pt);

        pd[pdi] = MM_PHYS(pt) | (1 << 2) | (1 << 1) | 1;
    } else {
        pt = (mm_pagetab_t) MM_VIRTUALIZE(pd[pdi] & ~0xFFF);
    }

    if (!(flags & (1 << 31))) {
        assert(!(pt[pti] & 1), "Entry already present for %p\n", virt_addr);
    }

#if defined(KERNEL_TEST_MODE)
    kdebug("map %p -> %p %cr%c\n", virt_addr, phys, (flags & (1 << 2)) ? 'u' : '-', (flags & (1 << 1)) ? 'w' : '-');
#endif
    pt[pti] = (phys & ~0xFFF) | (flags & 0xFFF) | 1;
    asm volatile("invlpg (%0)"::"a"(virt_addr):"memory");

    return 0;
}

int mm_map_pages_contiguous(mm_space_t pml4, uintptr_t virt_base, uintptr_t phys_base, size_t count, uint32_t flags) {
    for (size_t i = 0; i < count; ++i) {
        uintptr_t virt_addr = virt_base + (i << 12);

        if (amd64_map_single(pml4, virt_addr, phys_base + (i << 12), flags) != 0) {
            return -1;
        }
    }

    return 0;
}

int mm_space_clone(mm_space_t dst_pml4, const mm_space_t src_pml4, uint32_t flags) {
    if ((flags & MM_CLONE_FLG_USER)) {
        panic("NYI\n");
    }

    if ((flags & MM_CLONE_FLG_KERNEL)) {
        // Kernel table references may be cloned verbatim, as they're guarannteed to be
        // shared across all the spaces.
        // This allows to save some resources on allocating the actual PDPT/PD/PTs
        for (size_t i = AMD64_PML4I_USER_END; i < 512; ++i) {
            dst_pml4[i] = src_pml4[i];
        }
    }

    return 0;
}

int mm_space_fork(mm_space_t dst_pml4, const mm_space_t src_pml4, uint32_t flags) {
    if (flags & MM_CLONE_FLG_USER) {
        // Copy user pages:
        // TODO: CoW
        for (size_t pml4i = 0; pml4i < AMD64_PML4I_USER_END; ++pml4i) {
            if (!(src_pml4[pml4i] & 1)) {
                continue;
            }

            if (src_pml4[pml4i] & (1 << 7)) {
                panic("PML4 page has PS bit set\n");
            }

            mm_pdpt_t src_pdpt = (mm_pdpt_t) MM_VIRTUALIZE(src_pml4[pml4i] & ~0xFFF);
            // Make sure we've got a clean table
            _assert(!(dst_pml4[pml4i] & 1));
            mm_pdpt_t dst_pdpt = (mm_pdpt_t) amd64_mm_pool_alloc();
            _assert(dst_pdpt);
            dst_pml4[pml4i] = MM_PHYS(dst_pdpt) | (src_pml4[pml4i] & 0xFFF);

            for (size_t pdpti = 0; pdpti < 512; ++pdpti) {
                if (!(src_pdpt[pdpti] & 1)) {
                    continue;
                }

                if (src_pdpt[pdpti] & (1 << 7)) {
                    // Not allowed in U/S
                    panic("1GiB pages not supported in userspace\n");
                }

                mm_pagedir_t src_pd = (mm_pagedir_t) MM_VIRTUALIZE(src_pdpt[pdpti] & ~0xFFF);
                mm_pagedir_t dst_pd = (mm_pagedir_t) amd64_mm_pool_alloc();
                _assert(dst_pd);
                dst_pdpt[pdpti] = MM_PHYS(dst_pd) | (src_pdpt[pdpti] & 0xFFF);

                for (size_t pdi = 0; pdi < 512; ++pdi) {
                    if (!(src_pd[pdi] & 1)) {
                        continue;
                    }

                    if (src_pd[pdi] & (1 << 7)) {
                        panic("2MiB pages not supported in userspace\n");
                    }

                    mm_pagetab_t src_pt = (mm_pagetab_t) MM_VIRTUALIZE(src_pd[pdi] & ~0xFFF);
                    mm_pagetab_t dst_pt = (mm_pagetab_t) amd64_mm_pool_alloc();
                    _assert(dst_pt);
                    dst_pd[pdi] = MM_PHYS(dst_pt) | (src_pd[pdi] & 0xFFF);

                    for (size_t pti = 0; pti < 512; ++pti) {
                        if (!(src_pt[pti] & 1)) {
                            continue;
                        }

                        uintptr_t src_page_phys = src_pt[pti] & ~0xFFF;
                        uintptr_t dst_page_phys = amd64_phys_alloc_page();
                        _assert(dst_page_phys != MM_NADDR);

                        // SLOOOOOW UUUSEE COOOOW
                        memcpy((void *) MM_VIRTUALIZE(dst_page_phys),
                               (const void *) MM_VIRTUALIZE(src_page_phys),
                               0x1000);
                        kdebug("Cloning %p <- %p\n", dst_page_phys, src_page_phys);

                        dst_pt[pti] = dst_page_phys | (src_pt[pti] & 0xFFF);
                    }
                }
            }
        }
    }

    // Kernel pages don't need to be copied - just use mm_space_clone(, , MM_CLONE_FLG_KERNEL)
    return mm_space_clone(dst_pml4, src_pml4, MM_CLONE_FLG_KERNEL & flags);
}

void mm_space_release(mm_space_t pml4) {
    for (size_t pml4i = 0; pml4i < AMD64_PML4I_USER_END; ++pml4i) {
        if (!(pml4[pml4i] & 1)) {
            continue;
        }

        mm_pdpt_t pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & ~0xFFF);

        for (size_t pdpti = 0; pdpti < 512; ++pdpti) {
            if (!(pdpt[pdpti] & 1)) {
                continue;
            }

            mm_pagedir_t pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & ~0xFFF);

            for (size_t pdi = 0; pdi < 512; ++pdi) {
                if (!(pd[pdi] & 1)) {
                    continue;
                }

                mm_space_t pt = (mm_space_t) MM_VIRTUALIZE(pd[pdi] & ~0xFFF);

                for (size_t pti = 0; pti < 512; ++pti) {
                    if (!(pt[pti] & 1)) {
                        continue;
                    }

                    uintptr_t page_phys = pt[pti] & ~0xFFF;
                    amd64_phys_free(page_phys);
                }

                amd64_mm_pool_free(pt);
            }

            amd64_mm_pool_free(pd);
        }

        amd64_mm_pool_free(pdpt);

        pml4[pml4i] = 0;
    }
}

void mm_space_free(mm_space_t pml4) {
    mm_space_release(pml4);
    amd64_mm_pool_free(pml4);
}

static void amd64_mm_describe_range(const mm_space_t pml4, uintptr_t start_addr, uintptr_t end_addr) {
    uintptr_t addr = AMD64_MM_STRIPSX(start_addr);

    size_t range_length = 0;
    uintptr_t virt_range_begin = MM_NADDR;
    uint32_t range_flags = 0;

    mm_pdpt_t pdpt;

    while (addr < AMD64_MM_STRIPSX(end_addr)) {
        size_t pml4i = (addr >> 39) & 0x1FF;
        size_t pdpti = (addr >> 30) & 0x1FF;
        size_t pdi = (addr >> 21) & 0x1FF;
        size_t pti = (addr >> 12) & 0x1FF;
        size_t page_size = 1ULL << 39;

        if (pml4[pml4i] & 1) {
            if (pml4[pml4i] & (1 << 7)) {
                panic("Found a huge page in PML4 (shouldn't be possible): %p\n", AMD64_MM_ADDRSX(addr));
            }

            page_size = 1ULL << 30;
            pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & ~0xFFF);

            if (pdpt[pdpti] & 1) {
                if (pdpt[pdpti] & (1 << 7)) {
                    if (virt_range_begin == MM_NADDR) {
                        virt_range_begin = addr;
                        range_length = 0;
                        range_flags = pdpt[pdpti] & 5;
                    }

                    goto found;
                }

                panic("TODO: implement smaller page description\n");
            }
        }

        if (virt_range_begin != MM_NADDR) {
            kdebug("Range %p .. %p (%c%c) %S\n",
                virt_range_begin,
                virt_range_begin + range_length,
                (range_flags & (1 << 2) ? 'u' : '-'),
                (range_flags & (1 << 1) ? 'w' : '-'),
                range_length);
        }

found:
        addr += page_size;
        range_length += page_size;
    }

    if (virt_range_begin != MM_NADDR) {
        kdebug("Range %p .. %p (%c%c) %S\n",
            virt_range_begin,
            virt_range_begin + range_length,
            (range_flags & (1 << 2) ? 'u' : '-'),
            (range_flags & (1 << 1) ? 'w' : '-'),
            range_length);
    }
}

void mm_describe(const mm_space_t pml4) {
    kdebug("Memory space V:%p:\n", pml4);

    mm_pdpt_t pdpt;
    mm_pagedir_t pd;
    mm_pagetab_t pt;

    // Dump everything except kernel-space mappings
    kdebug("- Userspace:\n");
    amd64_mm_describe_range(pml4, 0, 0xFFFFFF0000000000);
    kdebug("- Kernelspace:\n");
    amd64_mm_describe_range(pml4, 0xFFFFFF0000000000, 0xFFFFFF00FFFFFFFF);
}
