#include "arch/amd64/mm/pool.h"
#include "arch/amd64/mm/map.h"
#include "sys/mem/shmem.h"
#include "sys/mem/phys.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/mm.h"

uintptr_t mm_map_get(const mm_space_t pml4, uintptr_t vaddr, uint64_t *flags) {
    vaddr = AMD64_MM_STRIPSX(vaddr);
    size_t pml4i = (vaddr >> MM_PML4I_SHIFT) & MM_PTE_INDEX_MASK;
    size_t pdpti = (vaddr >> MM_PDPTI_SHIFT) & MM_PTE_INDEX_MASK;
    size_t pdi =   (vaddr >> MM_PDI_SHIFT)   & MM_PTE_INDEX_MASK;
    size_t pti =   (vaddr >> MM_PTI_SHIFT)   & MM_PTE_INDEX_MASK;

    mm_pdpt_t pdpt;
    mm_pagedir_t pd;
    mm_pagetab_t pt;

    // L4:
    if (!(pml4[pml4i] & MM_PAGE_PRESENT)) {
        return MM_NADDR;
    }

    if (pml4[pml4i] & MM_PAGE_HUGE) {
        panic("NYI\n");
    }

    // L3:
    pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & MM_PTE_MASK);

    if (!(pdpt[pdpti] & MM_PAGE_PRESENT)) {
        return MM_NADDR;
    }

    if (pdpt[pdpti] & MM_PAGE_HUGE) {
        return (pdpt[pdpti] & MM_PTE_MASK) | (vaddr & MM_PAGE_L3_OFFSET_MASK);
    }

    // L2:
    pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & MM_PTE_MASK);

    if (!(pd[pdi] & MM_PAGE_PRESENT)) {
        return MM_NADDR;
    }

    if (pd[pdi] & MM_PAGE_HUGE) {
        // Page size is 2MiB (1 << 21)
        return (pd[pti] & MM_PTE_MASK) | (vaddr & MM_PAGE_L2_OFFSET_MASK);
    }

    // L1:
    pt = (mm_pagetab_t) MM_VIRTUALIZE(pd[pdi] & MM_PTE_MASK);

    if (!(pt[pti] & MM_PAGE_PRESENT)) {
        return MM_NADDR;
    }

    if (flags) {
        *flags = pt[pti] & MM_PTE_FLAGS_MASK;
    }

    return (pt[pti] & MM_PTE_MASK) | (vaddr & MM_PAGE_OFFSET_MASK);
}

uintptr_t mm_umap_single(mm_space_t pml4, uintptr_t vaddr, uint32_t size) {
    vaddr = AMD64_MM_STRIPSX(vaddr);
    // TODO: support page sizes other than 4KiB
    // (Though I can't think of any reason to use it)
    size_t pml4i = (vaddr >> MM_PML4I_SHIFT) & MM_PTE_INDEX_MASK;
    size_t pdpti = (vaddr >> MM_PDPTI_SHIFT) & MM_PTE_INDEX_MASK;
    size_t pdi =   (vaddr >> MM_PDI_SHIFT)   & MM_PTE_INDEX_MASK;
    size_t pti =   (vaddr >> MM_PTI_SHIFT)   & MM_PTE_INDEX_MASK;

    mm_pdpt_t pdpt;
    mm_pagedir_t pd;
    mm_pagetab_t pt;

    // L4:
    if (!(pml4[pml4i] & MM_PAGE_PRESENT)) {
        return MM_NADDR;
    }

    if (pml4[pml4i] & MM_PAGE_HUGE) {
        panic("NYI\n");
    }

    // L3:
    pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & MM_PTE_MASK);

    if (!(pdpt[pdpti] & MM_PAGE_PRESENT)) {
        return MM_NADDR;
    }

    if (pdpt[pdpti] & MM_PAGE_HUGE) {
        panic("NYI\n");
    }

    // L2:
    pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & MM_PTE_MASK);

    if (!(pd[pdi] & MM_PAGE_PRESENT)) {
        return MM_NADDR;
    }

    if (pd[pdi] & MM_PAGE_HUGE) {
        panic("NYI\n");
    }

    // L1:
    pt = (mm_pagetab_t) MM_VIRTUALIZE(pd[pdi] & MM_PTE_MASK);

    if (!(pt[pti] & MM_PAGE_PRESENT)) {
        return MM_NADDR;
    }

    uint64_t old = pt[pti] & MM_PTE_MASK;
    pt[pti] = 0;

    asm volatile("invlpg (%0)"::"r"(vaddr));
    struct page *page = PHYS2PAGE(old);
    _assert(page);
    _assert(page->refcount);
    --page->refcount;

    return old;
}

int mm_map_single(mm_space_t pml4, uintptr_t virt_addr, uintptr_t phys, uint64_t flags, int usage) {
    virt_addr = AMD64_MM_STRIPSX(virt_addr);
    // TODO: support page sizes other than 4KiB
    // (Though I can't think of any reason to use it)
    size_t pml4i = (virt_addr >> MM_PML4I_SHIFT) & MM_PTE_INDEX_MASK;
    size_t pdpti = (virt_addr >> MM_PDPTI_SHIFT) & MM_PTE_INDEX_MASK;
    size_t pdi =   (virt_addr >> MM_PDI_SHIFT)   & MM_PTE_INDEX_MASK;
    size_t pti =   (virt_addr >> MM_PTI_SHIFT)   & MM_PTE_INDEX_MASK;

    mm_pdpt_t pdpt;
    mm_pagedir_t pd;
    mm_pagetab_t pt;

    if (!(pml4[pml4i] & MM_PAGE_PRESENT)) {
        // Allocate PDPT
        pdpt = (mm_pdpt_t) amd64_mm_pool_alloc();
        assert(pdpt, "PDPT alloc failed\n");
        kdebug("Allocated PDPT = %p\n", pdpt);

        pml4[pml4i] = MM_PHYS(pdpt) |
                      MM_PAGE_PRESENT |
                      MM_PAGE_USER |
                      MM_PAGE_WRITE;
    } else {
        pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & MM_PTE_MASK);
    }

    if (!(pdpt[pdpti] & MM_PAGE_PRESENT)) {
        // Allocate PD
        pd = (mm_pagedir_t) amd64_mm_pool_alloc();
        assert(pd, "PD alloc failed\n");
        kdebug("Allocated PD = %p\n", pd);

        pdpt[pdpti] = MM_PHYS(pd) |
                      MM_PAGE_PRESENT |
                      MM_PAGE_USER |
                      MM_PAGE_WRITE;
    } else {
        pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & MM_PTE_MASK);
    }

    if (!(pd[pdi] & MM_PAGE_PRESENT)) {
        // Allocate PT
        pt = (mm_pagetab_t) amd64_mm_pool_alloc();
        assert(pt, "PT alloc failed\n");
        kdebug("Allocated PT = %p\n", pt);

        pd[pdi] = MM_PHYS(pt) |
                  MM_PAGE_PRESENT |
                  MM_PAGE_USER |
                  MM_PAGE_WRITE;
    } else {
        pt = (mm_pagetab_t) MM_VIRTUALIZE(pd[pdi] & MM_PTE_MASK);
    }

    // Disallow overwriting without unmapping entries first
    assert(!(pt[pti] & MM_PAGE_PRESENT), "Entry already present for %p\n", virt_addr);

#if defined(KERNEL_TEST_MODE)
    kdebug("map %p -> %p %cr%c%c%c\n", virt_addr, phys,
            (flags & MM_PAGE_USER) ? 'u' : '-',
            (flags & MM_PAGE_WRITE) ? 'w' : '-',
            (flags & MM_PAGE_NOEXEC) ? '-' : 'x',
            (flags & MM_PAGE_GLOBAL) ? 'G' : '-');
#endif

    // Increase refcount on physical page
    struct page *pg = PHYS2PAGE(phys);
    _assert(pg);
    ++pg->refcount;
    pg->usage = usage;

    pt[pti] = (phys & MM_PAGE_MASK) |
              (flags & MM_PTE_FLAGS_MASK) |
              MM_PAGE_PRESENT;

    asm volatile("invlpg (%0)"::"r"(virt_addr));

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
        for (size_t i = AMD64_PML4I_USER_END; i < MM_PTE_COUNT; ++i) {
            dst_pml4[i] = src_pml4[i];
        }
    }

    return 0;
}

int mm_space_fork(struct thread *dst, const struct thread *src, uint32_t flags) {
    mm_pml4_t dst_pml4 = dst->space;
    const mm_pml4_t src_pml4 = src->space;

    if (flags & MM_CLONE_FLG_USER) {
        for (size_t pml4i = 0; pml4i < AMD64_PML4I_USER_END; ++pml4i) {
            if (!(src_pml4[pml4i] & MM_PAGE_PRESENT)) {
                continue;
            }

            if (src_pml4[pml4i] & MM_PAGE_HUGE) {
                panic("PML4 page has PS bit set\n");
            }

            mm_pdpt_t src_pdpt = (mm_pdpt_t) MM_VIRTUALIZE(src_pml4[pml4i] & MM_PTE_MASK);
            // Make sure we've got a clean table
            _assert(!(dst_pml4[pml4i] & MM_PAGE_PRESENT));
            mm_pdpt_t dst_pdpt = (mm_pdpt_t) amd64_mm_pool_alloc();
            _assert(dst_pdpt);
            dst_pml4[pml4i] = MM_PHYS(dst_pdpt) | (src_pml4[pml4i] & MM_PTE_FLAGS_MASK);

            for (size_t pdpti = 0; pdpti < MM_PTE_COUNT; ++pdpti) {
                if (!(src_pdpt[pdpti] & MM_PAGE_PRESENT)) {
                    continue;
                }

                if (src_pdpt[pdpti] & MM_PAGE_HUGE) {
                    // Not allowed in U/S
                    panic("1GiB pages not supported in userspace\n");
                }

                mm_pagedir_t src_pd = (mm_pagedir_t) MM_VIRTUALIZE(src_pdpt[pdpti] & MM_PTE_MASK);
                mm_pagedir_t dst_pd = (mm_pagedir_t) amd64_mm_pool_alloc();
                _assert(dst_pd);
                dst_pdpt[pdpti] = MM_PHYS(dst_pd) | (src_pdpt[pdpti] & MM_PTE_FLAGS_MASK);

                for (size_t pdi = 0; pdi < MM_PTE_COUNT; ++pdi) {
                    if (!(src_pd[pdi] & MM_PAGE_PRESENT)) {
                        continue;
                    }

                    if (src_pd[pdi] & MM_PAGE_HUGE) {
                        panic("2MiB pages not supported in userspace\n");
                    }

                    mm_pagetab_t src_pt = (mm_pagetab_t) MM_VIRTUALIZE(src_pd[pdi] & MM_PTE_MASK);
                    mm_pagetab_t dst_pt = (mm_pagetab_t) amd64_mm_pool_alloc();
                    _assert(dst_pt);
                    dst_pd[pdi] = MM_PHYS(dst_pt) | (src_pd[pdi] & MM_PTE_FLAGS_MASK);

                    for (size_t pti = 0; pti < MM_PTE_COUNT; ++pti) {
                        if (!(src_pt[pti] & MM_PAGE_PRESENT)) {
                            continue;
                        }

                        uintptr_t src_page_virt = (pml4i << MM_PML4I_SHIFT) |
                                                  (pdpti << MM_PDPTI_SHIFT) |
                                                  (pdi << MM_PDI_SHIFT) |
                                                  (pti << MM_PTI_SHIFT);
                        uintptr_t src_page_phys = src_pt[pti] & MM_PTE_MASK;
                        struct page *src_page = PHYS2PAGE(src_page_phys);

                        _assert(src_page->refcount);
                        ++src_page->refcount;

                        if ((src_pt[pti] & MM_PAGE_WRITE) && src_page->usage == PU_PRIVATE) {
                            // Clone the mapping, use CoW
                            uint64_t access = src_pt[pti] & (MM_PTE_FLAGS_MASK & ~MM_PAGE_WRITE);
                            dst_pt[pti] = src_page_phys | access;
                            src_pt[pti] &= ~MM_PAGE_WRITE;
                            asm volatile("invlpg (%0)"::"r"(src_page_virt));
                        } else {
                            // Just clone the mapping - it's readonly
                            dst_pt[pti] = src_page_phys | (src_pt[pti] & MM_PTE_FLAGS_MASK);
                        }
                    }
                }
            }
        }
    }

    // Kernel pages don't need to be copied - just use mm_space_clone(, , MM_CLONE_FLG_KERNEL)
    return mm_space_clone(dst_pml4, src_pml4, MM_CLONE_FLG_KERNEL & flags);
}

void mm_space_release(struct thread *thr) {
    mm_space_t pml4 = thr->space;

    if (pml4 == mm_kernel) {
        panic("???\n");
    }

    for (size_t pml4i = 0; pml4i < AMD64_PML4I_USER_END; ++pml4i) {
        if (!(pml4[pml4i] & MM_PAGE_PRESENT)) {
            continue;
        }

        mm_pdpt_t pdpt = (mm_pdpt_t) MM_VIRTUALIZE(pml4[pml4i] & MM_PTE_MASK);

        for (size_t pdpti = 0; pdpti < MM_PTE_COUNT; ++pdpti) {
            if (!(pdpt[pdpti] & MM_PAGE_PRESENT)) {
                continue;
            }

            mm_pagedir_t pd = (mm_pagedir_t) MM_VIRTUALIZE(pdpt[pdpti] & MM_PTE_MASK);

            for (size_t pdi = 0; pdi < MM_PTE_COUNT; ++pdi) {
                if (!(pd[pdi] & MM_PAGE_PRESENT)) {
                    continue;
                }

                mm_space_t pt = (mm_space_t) MM_VIRTUALIZE(pd[pdi] & MM_PTE_MASK);

                for (size_t pti = 0; pti < MM_PTE_COUNT; ++pti) {
                    if (!(pt[pti] & MM_PAGE_PRESENT)) {
                        continue;
                    }

                    uintptr_t page_phys = pt[pti] & MM_PTE_MASK;
                    struct page *page = PHYS2PAGE(page_phys);
                    _assert(page->refcount);
                    --page->refcount;

                    // Any page with zero refcount can be released
                    if (!page->refcount) {
                        mm_phys_free_page(page_phys);
                    }
                }

                amd64_mm_pool_free(pt);
            }

            amd64_mm_pool_free(pd);
        }

        amd64_mm_pool_free(pdpt);

        pml4[pml4i] = 0;
    }
}

void mm_space_free(struct thread *thr) {
    mm_space_release(thr);
    amd64_mm_pool_free(thr->space);
}

void mm_describe(const mm_space_t pml4) {
    kwarn("mm_describe was removed until I reimplement it properly\n");
}
