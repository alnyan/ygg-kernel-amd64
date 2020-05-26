#include "arch/amd64/mm/phys.h"
#include "arch/amd64/mm/pool.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "sys/spin.h"
#include "sys/mem/phys.h"
#include "sys/mm.h"

extern int _kernel_end_phys;
// Roughly 36MiB of lower memory is occupied by kernel so far:
// The rest is available for both kernel and user allocation
#define PHYS_ALLOWED_BEGIN          ((((uintptr_t) &_kernel_end_phys + MM_POOL_SIZE) + 0xFFF) & ~0xFFF)
#define PHYS_ALLOC_START_INDEX      (PHYS_ALLOWED_BEGIN / MM_PAGE_SIZE)
#define PHYS_MAX_PAGES              ((1U << 30) / 0x1000)

static struct page _pages[PHYS_MAX_PAGES];
static size_t _total_pages = 0, _alloc_pages = 0;
struct page *mm_pages = _pages;

void amd64_phys_stat(struct amd64_phys_stat *st) {
    st->limit = _total_pages * MM_PAGE_SIZE;
    st->pages_free = _total_pages - _alloc_pages;
    st->pages_used = _alloc_pages;
}

uintptr_t mm_phys_alloc_page(void) {
    for (size_t i = PHYS_ALLOC_START_INDEX; i < PHYS_MAX_PAGES; ++i) {
        if (!(_pages[i].flags & PG_ALLOC)) {
            _assert(!_pages[i].refcount);
            _pages[i].flags |= PG_ALLOC;
            ++_alloc_pages;
            return i * MM_PAGE_SIZE;
        }
    }
    return MM_NADDR;
}

void mm_phys_free_page(uintptr_t addr) {
    struct page *page = PHYS2PAGE(addr);
    _assert(page);
    _assert(!page->refcount);
    _assert(page->flags & PG_ALLOC);
    --_alloc_pages;
    page->flags &= ~PG_ALLOC;
}

uintptr_t mm_phys_alloc_contiguous(size_t count) {
    for (size_t i = PHYS_ALLOC_START_INDEX; i < PHYS_MAX_PAGES - count; ++i) {
        for (size_t j = 0; j < count; ++j) {
            if (_pages[i + j].flags & PG_ALLOC) {
                goto fail;
            }
        }

        for (size_t j = 0; j < count; ++j) {
            _assert(!_pages[i + j].refcount);
            _pages[i + j].flags |= PG_ALLOC;
        }

        return i * MM_PAGE_SIZE;
fail:
        continue;
    }
    return MM_NADDR;
}

static void phys_add_page(size_t index) {
    _pages[index].flags &= ~PG_ALLOC;
    _pages[index].refcount = 0;
}

void amd64_phys_memory_map(const struct multiboot_tag_mmap *mmap) {
    kdebug("Kernel table pool ends @ %p\n", PHYS_ALLOWED_BEGIN);
    kdebug("Memory map @ %p\n", mmap);

    size_t item_offset = offsetof(struct multiboot_tag_mmap, entries);

    memset(_pages, 0xFF, sizeof(_pages));
    _total_pages = 0;
    _alloc_pages = 0;

    // Collect usable physical memory information
    while (item_offset < mmap->size) {
        //const multiboot_memory_map_t *entry = (const multiboot_memory_map_t *) (curr_item);
        const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *) ((uintptr_t) mmap + item_offset);
        uintptr_t page_aligned_begin = MAX((entry->addr + 0xFFF) & ~0xFFF, PHYS_ALLOWED_BEGIN);
        uintptr_t page_aligned_end = (entry->addr + entry->len) & ~0xFFF;

        if (entry->type == 1 && page_aligned_end > page_aligned_begin) {
            kdebug("+++ %S @ %p\n", page_aligned_end - page_aligned_begin, page_aligned_begin);

            for (uintptr_t addr = page_aligned_begin - PHYS_ALLOWED_BEGIN;
                 addr < (page_aligned_end - PHYS_ALLOWED_BEGIN); addr += 0x1000) {
                size_t index = addr / MM_PAGE_SIZE;
                _assert(index < PHYS_MAX_PAGES);
                phys_add_page(index);
                ++_total_pages;
            }
        }

        item_offset += mmap->entry_size;
    }

    kdebug("%S available\n", _total_pages << 12);
}
