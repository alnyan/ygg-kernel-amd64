#include "arch/amd64/mm/map.h"
#include "sys/mem/vmalloc.h"
#include "sys/mem/phys.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/panic.h"

uintptr_t vmfind(const mm_space_t pml4, uintptr_t from, uintptr_t to, size_t npages) {
    // XXX: The slowest approach I could think of
    //      Though the easiest one
    size_t page_index = from / MM_PAGE_SIZE;

    while ((page_index + npages) <= (to / MM_PAGE_SIZE)) {
        for (size_t i = 0; i < npages; ++i) {
            if (mm_map_get(pml4, (page_index + i) * MM_PAGE_SIZE, NULL) != MM_NADDR) {
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

void vmfree(mm_space_t pml4, uintptr_t addr, size_t npages) {
    uintptr_t phys;
    for (size_t i = 0; i < npages; ++i) {
        if ((phys = mm_umap_single(pml4, addr + i * MM_PAGE_SIZE, 1)) == MM_NADDR) {
            panic("Double vmfree error: %p is not an allocated page\n", addr + i * MM_PAGE_SIZE);
        }
        mm_phys_free_page(phys);
    }
}
