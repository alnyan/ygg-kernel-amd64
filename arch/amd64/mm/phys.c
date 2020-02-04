#include "arch/amd64/mm/phys.h"
#include "arch/amd64/mm/pool.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "sys/spin.h"
#include "sys/mm.h"

extern int _kernel_end_phys;
// Roughly 36MiB of lower memory is occupied by kernel so far:
// The rest is available for both kernel and user allocation
#define PHYS_ALLOWED_BEGIN          ((((uintptr_t) &_kernel_end_phys + MM_POOL_SIZE) + 0xFFF) & ~0xFFF)

// TODO: move to sys/util.h
#define MAX(x, y)                   ((x) > (y) ? (x) : (y))

// TODO: this could be a better allocator
//     + Support more memory
// 1 index is 64 pages - 256KiB
#define PHYS_MAX_INDEX              16384       // Gives exactly 4GiB available for allocation
#define PHYS_TRACK_INDEX(page)      ((page) >> 18)
#define PHYS_TRACK_BIT(page)        (((page) >> 12) & 0x3F)

static uint64_t amd64_phys_memory_track[PHYS_MAX_INDEX];
static uint64_t amd64_phys_last_index = 0;
static uint64_t amd64_phys_ceiling = 0xFFFFFFFF;
static spin_t alloc_spin = 0;

void amd64_phys_stat(struct amd64_phys_stat *st) {
    uintptr_t irq;
    spin_lock_irqsave(&alloc_spin, &irq);

    st->pages_free = 0;
    st->pages_used = 0;
    st->limit = amd64_phys_ceiling;

    for (uint64_t i = 0; i < PHYS_MAX_INDEX; ++i) {
        if ((i << 18) >= amd64_phys_ceiling) {
            break;
        }

        for (uint64_t j = 0; j < 64; ++j) {
            if (((i << 18) | (j << 12)) >= amd64_phys_ceiling) {
                break;
            }

            if (amd64_phys_memory_track[i] & (1ULL << j)) {
                ++st->pages_used;
            } else {
                ++st->pages_free;
            }
        }
    }

    spin_release_irqrestore(&alloc_spin, &irq);
}

// Allocate a single 4K page
uintptr_t amd64_phys_alloc_page(void) {
    uintptr_t irq;
    spin_lock_irqsave(&alloc_spin, &irq);
    for (uint64_t i = amd64_phys_last_index; i < PHYS_MAX_INDEX; ++i) {
        for (uint64_t j = 0; j < 64; ++j) {
            if (!(amd64_phys_memory_track[i] & (1ULL << j))) {
                amd64_phys_memory_track[i] |= (1ULL << j);
                amd64_phys_last_index = i;
                spin_release_irqrestore(&alloc_spin, &irq);
                return PHYS_ALLOWED_BEGIN + ((i << 18) | (j << 12));
            }
        }
    }
    for (uint64_t i = 0; i < amd64_phys_last_index; ++i) {
        for (uint64_t j = 0; j < 64; ++j) {
            if (!(amd64_phys_memory_track[i] & (1ULL << j))) {
                amd64_phys_memory_track[i] |= (1ULL << j);
                amd64_phys_last_index = i;
                spin_release_irqrestore(&alloc_spin, &irq);
                return PHYS_ALLOWED_BEGIN + ((i << 18) | (j << 12));
            }
        }
    }

    spin_release_irqrestore(&alloc_spin, &irq);
    return MM_NADDR;
}

void amd64_phys_free(uintptr_t page) {
    uintptr_t irq;
    spin_lock_irqsave(&alloc_spin, &irq);

    // Address is too low
    assert(page >= PHYS_ALLOWED_BEGIN, "The page is outside the physical range: %p\n", page);
    page -= PHYS_ALLOWED_BEGIN;
    // Address is too high
    assert(PHYS_TRACK_INDEX(page) < PHYS_MAX_INDEX, "The page is outside the physical range: %p\n", page + PHYS_ALLOWED_BEGIN);

    uint64_t bit = 1ULL << PHYS_TRACK_BIT(page);
    uint64_t index = PHYS_TRACK_INDEX(page);

    // Double free error
    assert(amd64_phys_memory_track[index] & bit, "Double free error (phys): %p\n", page + PHYS_ALLOWED_BEGIN);

    amd64_phys_memory_track[index] &= ~bit;
    spin_release_irqrestore(&alloc_spin, &irq);
}

// XXX: very slow impl.
uintptr_t amd64_phys_alloc_contiguous(size_t count) {
    uintptr_t addr = 0;
    kdebug("Requested %S\n", count << 12);

    if (count >= (PHYS_MAX_INDEX << 6)) {
        // The requested range is too large
        return MM_NADDR;
    }
    uintptr_t irq;
    spin_lock_irqsave(&alloc_spin, &irq);

    while (addr < (((uint64_t) (PHYS_MAX_INDEX - (count >> 6) - 1)) << 18)) {
        // Check if we can allocate these pages
        for (size_t i = 0; i < count; ++i) {
            uintptr_t page = (i << 12) + addr;
            if (amd64_phys_memory_track[PHYS_TRACK_INDEX(page)] & (1ULL << PHYS_TRACK_BIT(page))) {
                // When reaching unavailable page, just start searching beyond its address
                addr = page + 0x1000;
                goto no_match;
            }
        }
        // All pages are available
        for (size_t i = 0; i < count; ++i) {
            uintptr_t page = (i << 12) + addr;
            amd64_phys_memory_track[PHYS_TRACK_INDEX(page)] |= (1ULL << PHYS_TRACK_BIT(page));
        }

        kdebug("== %p\n", addr + PHYS_ALLOWED_BEGIN);
        spin_release_irqrestore(&alloc_spin, &irq);
        return PHYS_ALLOWED_BEGIN + addr;

        // Found unavailable page in the range, continue searching
no_match:
        continue;
    }

    spin_release_irqrestore(&alloc_spin, &irq);
    return MM_NADDR;
}

void amd64_phys_memory_map(const multiboot_memory_map_t *mmap, size_t length) {
    kdebug("Kernel table pool ends @ %p\n", PHYS_ALLOWED_BEGIN);
    kdebug("Memory map @ %p\n", mmap);
    memset(amd64_phys_memory_track, 0xFF, sizeof(amd64_phys_memory_track));
    uintptr_t curr_item = (uintptr_t) mmap;
    uintptr_t mmap_end = length + curr_item;
    size_t total_phys = 0;

    // Collect usable physical memory information
    while (curr_item < mmap_end) {
        const multiboot_memory_map_t *entry = (const multiboot_memory_map_t *) (curr_item);

        uintptr_t page_aligned_begin = MAX((entry->addr + 0xFFF) & ~0xFFF, PHYS_ALLOWED_BEGIN);
        uintptr_t page_aligned_end = (entry->addr + entry->len) & ~0xFFF;

        if (entry->type == 1 && page_aligned_end > page_aligned_begin) {
            kdebug("+++ %S @ %p\n", page_aligned_end - page_aligned_begin, page_aligned_begin);

            for (uintptr_t addr = page_aligned_begin - PHYS_ALLOWED_BEGIN;
                 addr < (page_aligned_end - PHYS_ALLOWED_BEGIN); addr += 0x1000) {
                uintptr_t index = PHYS_TRACK_INDEX(addr);
                if (index >= PHYS_MAX_INDEX) {
                    kdebug("Too high: %p\n", addr);
                    break;
                }
                amd64_phys_memory_track[index] &= ~(1ULL << PHYS_TRACK_BIT(addr));
                ++total_phys;
                amd64_phys_ceiling = addr + 0x1000;
            }
        }

        curr_item += entry->size + sizeof(uint32_t);
    }

    kdebug("%S available\n", total_phys << 12);
}
