#include "sys/amd64/mm/phys.h"
#include "sys/amd64/mm/map.h"
#include "sys/assert.h"
#include "sys/vmalloc.h"
#include "sys/debug.h"
#include "sys/panic.h"

uintptr_t vmfind(const mm_space_t pml4, uintptr_t from, uintptr_t to, size_t npages) {
    // XXX: The slowest approach I could think of
    //      Though the easiest one
    size_t page_index = from / MM_PAGE_SIZE;

    while ((page_index + npages) <= (to / MM_PAGE_SIZE)) {
        for (size_t i = 0; i < npages; ++i) {
            if (amd64_map_get(pml4, (page_index + i) * MM_PAGE_SIZE, 0) != MM_NADDR) {
                goto no_match;
            }
        }

        return page_index * MM_PAGE_SIZE;
no_match:
        ++page_index;
        continue;
    }

    return MM_NADDR;
}

uintptr_t vmalloc(mm_space_t pml4, uintptr_t from, uintptr_t to, size_t npages, uint64_t flags) {
    uintptr_t addr = vmfind(pml4, from, to, npages);
    uintptr_t virt_page, phys_page;
    uint64_t rflags = flags & (MM_PAGE_USER | MM_PAGE_WRITE | MM_PAGE_NOEXEC);

    if (addr == MM_NADDR) {
        return MM_NADDR;
    }

    for (size_t i = 0; i < npages; ++i) {
        virt_page = addr + i * MM_PAGE_SIZE;
        phys_page = amd64_phys_alloc_page();

        // Allocation of physical page failed, clean up
        if (phys_page == MM_NADDR) {
            // Unmap previously allocated pages
            for (size_t j = 0; j < i; ++j) {
                virt_page = addr + j * MM_PAGE_SIZE;
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
        assert(amd64_map_single(pml4, virt_page, phys_page, rflags) == 0, "Failed to map page: %p\n", virt_page);
    }

    return addr;
}

void vmfree(mm_space_t pml4, uintptr_t addr, size_t npages) {
    uintptr_t phys;
    for (size_t i = 0; i < npages; ++i) {
        if ((phys = amd64_map_umap(pml4, addr + i * MM_PAGE_SIZE, 1)) == MM_NADDR) {
            panic("Double vmfree error: %p is not an allocated page\n", addr + i * MM_PAGE_SIZE);
        }
        amd64_phys_free(phys);
    }
}
