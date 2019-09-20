#include "arch/amd64/mm/phys.h"
#include "arch/amd64/mm/map.h"
#include "sys/assert.h"
#include "sys/vmalloc.h"
#include "sys/debug.h"
#include "sys/panic.h"

uintptr_t vmfind(const mm_space_t pml4, uintptr_t from, uintptr_t to, size_t npages) {
    // XXX: The slowest approach I could think of
    //      Though the easiest one
    size_t page_index = from >> 12;

    while ((page_index + npages) <= (to >> 12)) {
        for (size_t i = 0; i < npages; ++i) {
            if (amd64_map_get(pml4, (page_index + i) << 12, 0) != MM_NADDR) {
                goto no_match;
            }
        }

        return page_index << 12;
no_match:
        ++page_index;
        continue;
    }

    return MM_NADDR;
}

uintptr_t vmalloc(mm_space_t pml4, uintptr_t from, uintptr_t to, size_t npages, int flags) {
    uintptr_t addr = vmfind(pml4, from, to, npages);
    uintptr_t virt_page, phys_page;
    uint32_t rflags = 0;

    if (flags & VM_ALLOC_WRITE) {
        rflags |= 1 << 1;
    }
    if (flags & VM_ALLOC_USER) {
        rflags |= 1 << 2;
    }

    if (addr == MM_NADDR) {
        return MM_NADDR;
    }

    for (size_t i = 0; i < npages; ++i) {
        virt_page = addr + (i << 12);
        phys_page = amd64_phys_alloc_page();

        // Allocation of physical page failed, clean up
        if (phys_page == MM_NADDR) {
            // Unmap previously allocated pages
            for (size_t j = 0; j < i; ++j) {
                virt_page = addr + (j << 12);
                // Deallocate physical pages that've already been mapped
                // We've mapped only 4KiB pages, so expect to unmap only
                // 4KiB pages
                assert((phys_page = amd64_map_umap(pml4, virt_page, 1)) != MM_NADDR,
                        "Failed to deallocate page when cleaning up botched alloc: %p\n", virt_page);

                amd64_phys_free(phys_page);
            }
            return MM_NADDR;
        }

        // Succeeded, map the page
        assert(amd64_map_single(pml4, virt_page, phys_page, 0) == 0, "Failed to map page: %p\n", virt_page);
    }

    return addr;
}

void vmfree(mm_space_t pml4, uintptr_t addr, size_t npages) {
    uintptr_t phys;
    for (size_t i = 0; i < npages; ++i) {
        if ((phys = amd64_map_umap(pml4, addr + (i << 12), 1)) == MM_NADDR) {
            panic("Double vmfree error: %p is not an allocated page\n", addr + (i << 12));
        }
        amd64_phys_free(phys);
    }
}
