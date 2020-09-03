#include "arch/amd64/mm/phys.h"
#include "arch/amd64/mm/pool.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "sys/spin.h"
#include "sys/mem/phys.h"
#include "sys/mm.h"

#define PHYS_MAX_PAGES              ((1U << 30) / 0x1000)

// Reserve 1MiB at bottom
#define LOW_BOUND                   0x100000

#define MMAP_KIND_RESERVED          0
#define MMAP_KIND_USABLE            1
#define MMAP_KIND_UNKNOWN           2
struct mmap_iter {
    const struct mm_phys_memory_map *map;
    size_t position, limit;
};

struct page *mm_pages = NULL;
static size_t _total_pages, _pages_free;
static size_t _alloc_pages[_PU_COUNT];
static spin_t phys_spin = 0;
static struct mm_phys_reserved phys_reserve_mm_pages;
static LIST_HEAD(reserved_regions);

static int is_reserved(uintptr_t addr) {
    struct mm_phys_reserved *res;
    list_for_each_entry(res, &reserved_regions, link) {
        if (addr >= res->begin && addr < res->end) {
            return 1;
        }
    }

    return 0;
}

// EFI memory map
struct efi_mmap_entry {
    uint32_t type;
    uintptr_t physical_start;
    uintptr_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

enum efi_mmap_type {
    efi_reserved_memory_type,
    efi_loader_code,
    efi_loader_data,
    efi_boot_services_code,
    efi_boot_services_data,
    efi_runtime_services_code,
    efi_runtime_services_data,
    efi_conventional_memory,
    efi_unusable_memory,
    efi_acpi_reclaim_memory,
    efi_acpi_memory_nvs,
    efi_memory_mapped_io,
    efi_memory_mapped_io_portspace,
    efi_pal_code,
    efi_persistent_memory,
    efi_max_memory_type
};

static void mmap_iter_init(const struct mm_phys_memory_map *mmap,
                           struct mmap_iter *iter) {
    _assert(mmap->format == MM_PHYS_MMAP_FMT_YBOOT ||
            mmap->format == MM_PHYS_MMAP_FMT_MULTIBOOT2);
    iter->map = mmap;
    iter->limit = mmap->entry_count;
    iter->position = 0;
}

static int mmap_iter_next(struct mmap_iter *iter,
                          int *kind,
                          uintptr_t *base,
                          size_t *size) {
    const struct mm_phys_memory_map *map;

    map = iter->map;
    _assert(map);

    if (iter->position == iter->limit) {
        return 0;
    }

    switch (map->format) {
    case MM_PHYS_MMAP_FMT_YBOOT:
        {
            struct efi_mmap_entry *ent;
            ent = map->address + iter->position * map->entry_size;

            *base = ent->physical_start;
            *size = ent->number_of_pages * 0x1000;
            switch (ent->type) {
            case efi_loader_code:
            case efi_loader_data:
            case efi_boot_services_code:
            case efi_boot_services_data:
            case efi_runtime_services_code:
            case efi_runtime_services_data:
            case efi_conventional_memory:
                *kind = MMAP_KIND_USABLE;
                break;
            case efi_pal_code:
            case efi_persistent_memory:
            case efi_reserved_memory_type:
            case efi_unusable_memory:
            case efi_acpi_reclaim_memory:
            case efi_acpi_memory_nvs:
            case efi_memory_mapped_io_portspace:
            case efi_memory_mapped_io:
                *kind = MMAP_KIND_RESERVED;
                break;
            default:
                panic("Unknown ent type: %02x\n", ent->type);
            }
        }
        break;
    case MM_PHYS_MMAP_FMT_MULTIBOOT2:
        {
            struct multiboot_mmap_entry *ent;
            ent = map->address + iter->position * map->entry_size;

            *base = ent->addr;
            *size = ent->len;
            if (ent->type == MULTIBOOT_MEMORY_AVAILABLE) {
                *kind = MMAP_KIND_USABLE;
            } else {
                *kind = MMAP_KIND_RESERVED;
            }
        }
        break;
    default:
        panic("Impossible\n");
    }

    ++iter->position;
    return 1;
}

void mm_phys_reserve(struct mm_phys_reserved *res) {
    list_head_init(&res->link);
    list_add(&res->link, &reserved_regions);
    kdebug("#### Reserve region: %p .. %p\n", res->begin, res->end);
}

void mm_phys_stat(struct mm_phys_stat *st) {
    st->pages_total = _total_pages;
    st->pages_free = _pages_free;
    st->pages_used_kernel = _alloc_pages[PU_KERNEL];
    st->pages_used_user = _alloc_pages[PU_PRIVATE];
    st->pages_used_shared = _alloc_pages[PU_SHARED];
    st->pages_used_paging = _alloc_pages[PU_PAGING];
    st->pages_used_cache = _alloc_pages[PU_CACHE];
}

uint64_t *amd64_mm_pool_alloc(void) {
    uint64_t *table;
    uintptr_t ptr;

    ptr = mm_phys_alloc_page(PU_PAGING);
    _assert(ptr != MM_NADDR);

    table = (uint64_t *) MM_VIRTUALIZE(ptr);
    memset(table, 0, MM_PAGE_SIZE);
    return table;
}

void amd64_mm_pool_free(uint64_t *p) {
    memset(p, 0xFF, MM_PAGE_SIZE);
    mm_phys_free_page(MM_PHYS(p));
}

uintptr_t mm_phys_alloc_page(enum page_usage pu) {
    _assert(pu < _PU_COUNT && pu != PU_UNKNOWN);

    uintptr_t irq;
    spin_lock_irqsave(&phys_spin, &irq);

    for (size_t i = LOW_BOUND >> 12; i < PHYS_MAX_PAGES; ++i) {
        struct page *pg = &mm_pages[i];

        if (!(pg->flags & PG_ALLOC)) {
            _assert(pg->usage == PU_UNKNOWN);
            _assert(pg->refcount == 0);
            pg->usage = pu;
            pg->flags |= PG_ALLOC;
            ++_alloc_pages[pu];
            _assert(_pages_free);
            --_pages_free;

            spin_release_irqrestore(&phys_spin, &irq);
            return i * MM_PAGE_SIZE;
        }
    }

    spin_release_irqrestore(&phys_spin, &irq);
    return MM_NADDR;
}

void mm_phys_free_page(uintptr_t addr) {
    uintptr_t irq;
    spin_lock_irqsave(&phys_spin, &irq);

    struct page *pg = PHYS2PAGE(addr);
    _assert(!pg->refcount);
    _assert(pg->flags & PG_ALLOC);

    _assert(_alloc_pages[pg->usage]);
    --_alloc_pages[pg->usage];
    ++_pages_free;

    pg->flags &= ~PG_ALLOC;
    pg->usage = PU_UNKNOWN;

    spin_release_irqrestore(&phys_spin, &irq);
}

uintptr_t mm_phys_alloc_contiguous(size_t count, enum page_usage pu) {
    uintptr_t irq;
    spin_lock_irqsave(&phys_spin, &irq);

    for (size_t i = LOW_BOUND >> 12; i < PHYS_MAX_PAGES - count; ++i) {
        for (size_t j = 0; j < count; ++j) {
            if (mm_pages[i + j].flags & PG_ALLOC) {
                goto fail;
            }
        }

        for (size_t j = 0; j < count; ++j) {
            _assert(!mm_pages[i + j].refcount);
            mm_pages[i + j].flags |= PG_ALLOC;
            mm_pages[i + j].usage = pu;

            ++_alloc_pages[pu];
            _assert(_pages_free);
            --_pages_free;
        }

        spin_release_irqrestore(&phys_spin, &irq);
        return i * MM_PAGE_SIZE;
fail:
        continue;
    }
    spin_release_irqrestore(&phys_spin, &irq);
    return MM_NADDR;
}

static uintptr_t place_mm_pages(const struct mm_phys_memory_map *mmap, size_t req_count) {
    struct mmap_iter iter;
    size_t item_offset;
    uintptr_t base;
    size_t size;
    int kind;

    mmap_iter_init(mmap, &iter);
    // TODO: merge two consecutive entries into a single address block
    while (mmap_iter_next(&iter, &kind, &base, &size)) {
        uintptr_t page_aligned_begin = (base + 0xFFF) & ~0xFFF;
        uintptr_t page_aligned_end = (base + size) & ~0xFFF;

        kdebug("Region: %p .. %p\n", page_aligned_begin, page_aligned_end);

        if (kind == MMAP_KIND_USABLE && page_aligned_end > page_aligned_begin) {
            // Something like mm_phys_alloc_contiguous does, but
            // we don't yet have it obviously
            size_t collected = 0;
            uintptr_t base_addr = MM_NADDR;

            for (uintptr_t addr = page_aligned_begin; addr < page_aligned_end; addr += 0x1000) {
                if (is_reserved(addr)) {
                    collected = 0;
                    base_addr = MM_NADDR;
                    continue;
                }

                if (base_addr == MM_NADDR) {
                    base_addr = addr;
                }
                ++collected;
                if (collected == req_count) {
                    return base_addr;
                }
            }
        }
    }

    return MM_NADDR;
}

void amd64_phys_memory_map(const struct mm_phys_memory_map *mmap) {
    struct mmap_iter iter;
    uintptr_t base;
    size_t size;
    int kind;

    // Allocate space for mm_pages array
    size_t mm_pages_req_count = (PHYS_MAX_PAGES * sizeof(struct page) + 0xFFF) >> 12;
    uintptr_t mm_pages_addr = place_mm_pages(mmap, mm_pages_req_count);
    _assert(mm_pages_addr != MM_NADDR);

    kdebug("Placing mm_pages (%u) at %p\n", mm_pages_req_count, mm_pages_addr);
    phys_reserve_mm_pages.begin = mm_pages_addr;
    phys_reserve_mm_pages.end = mm_pages_addr + mm_pages_req_count * MM_PAGE_SIZE;
    // TODO: also reserve memory map itself before screwing with it?
    mm_phys_reserve(&phys_reserve_mm_pages);

    mm_pages = (struct page *) MM_VIRTUALIZE(mm_pages_addr);
    for (size_t i = 0; i < PHYS_MAX_PAGES; ++i) {
        mm_pages[i].flags = PG_ALLOC;
        mm_pages[i].refcount = (size_t) -1L;
    }

    kdebug("Memory map @ %p\n", mmap);

    _total_pages = 0;
    mmap_iter_init(mmap, &iter);

    // Collect usable physical memory information
    while (mmap_iter_next(&iter, &kind, &base, &size)) {
        uintptr_t page_aligned_begin = (base + 0xFFF) & ~0xFFF;
        uintptr_t page_aligned_end = (base + size) & ~0xFFF;

        if (kind == MMAP_KIND_USABLE && page_aligned_end > page_aligned_begin) {
            kdebug("+++ %S @ %p\n", page_aligned_end - page_aligned_begin, page_aligned_begin);

            for (uintptr_t addr = page_aligned_begin; addr < page_aligned_end; addr += 0x1000) {
                extern char _kernel_end;

                if (!is_reserved(addr) && addr >= MM_PHYS(&_kernel_end)) {
                    struct page *pg = PHYS2PAGE(addr);
                    pg->flags &= ~PG_ALLOC;
                    pg->usage = PU_UNKNOWN;
                    pg->refcount = 0;
                    ++_total_pages;
                }
            }
        }
    }

    _pages_free = _total_pages;

    kdebug("%S available\n", _total_pages << 12);
}
