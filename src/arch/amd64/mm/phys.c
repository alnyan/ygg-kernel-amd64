#include "arch/amd64/mm/phys.h"
#include "arch/amd64/mm/pool.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/mem.h"
#include "sys/mm.h"

// Roughly 36MiB of lower memory is occupied by kernel so far:
// The rest is available for both kernel and user allocation
#define PHYS_ALLOWED_BEGIN          (((/* Kernel image end */ 0x400000 + MM_POOL_SIZE) + 0xFFF) & ~0xFFF)

// TODO: move to sys/util.h
#define MAX(x, y)                   ((x) > (y) ? (x) : (y))

// TODO: this could be a better allocator
//     + Support more memory
#define PHYS_MAX_INDEX              16384       // Gives exactly 4GiB available for allocation
#define PHYS_TRACK_INDEX(page)      ((page) >> 18)
#define PHYS_TRACK_BIT(page)        (((page) >> 12) & 0x3F)

static uint64_t amd64_phys_memory_track[PHYS_MAX_INDEX];
static uint64_t amd64_phys_last_index = 0;

// Allocate a single 4K page
uintptr_t amd64_phys_alloc_page(void) {
    for (uint64_t i = amd64_phys_last_index; i < PHYS_MAX_INDEX; ++i) {
        for (uint64_t j = 0; j < 64; ++j) {
            if (!(amd64_phys_memory_track[i] & (1ULL << j))) {
                amd64_phys_memory_track[i] |= (1ULL << j);
                amd64_phys_last_index = i;
                return PHYS_ALLOWED_BEGIN + ((i << 18) | (j << 12));
            }
        }
    }
    for (uint64_t i = 0; i < amd64_phys_last_index; ++i) {
        for (uint64_t j = 0; j < 64; ++j) {
            if (!(amd64_phys_memory_track[i] & (1ULL << j))) {
                amd64_phys_memory_track[i] |= (1ULL << j);
                amd64_phys_last_index = i;
                return PHYS_ALLOWED_BEGIN + ((i << 18) | (j << 12));
            }
        }
    }
    return MM_NADDR;
}

void amd64_phys_free(uintptr_t page) {
    // Address is too low
    if (page < PHYS_ALLOWED_BEGIN) {
        panic("Tried to free kernel physical pages\n");
    }
    // Address is too high
    if (PHYS_TRACK_INDEX(page) >= PHYS_MAX_INDEX) {
        panic("Tried to free non-available physical page\n");
    }

    uint64_t bit = 1ULL << PHYS_TRACK_BIT(page);
    uint64_t index = PHYS_TRACK_INDEX(page);

    // Double free error
    if (!(amd64_phys_memory_track[index] & bit)) {
        panic("Tried to free non-allocated physical page\n");
    }

    amd64_phys_memory_track[index] &= ~bit;
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
            // TODO: size pretty-printing
            kdebug("+++ %uKiB @ %p\n", (page_aligned_end - page_aligned_begin) / 1024, page_aligned_begin);

            for (uintptr_t addr = page_aligned_begin - PHYS_ALLOWED_BEGIN;
                 addr < (page_aligned_end - PHYS_ALLOWED_BEGIN); addr += 0x1000) {
                uintptr_t index = PHYS_TRACK_INDEX(addr);
                if (index >= PHYS_MAX_INDEX) {
                    kdebug("Too high: %p\n", addr);
                    break;
                }
                amd64_phys_memory_track[index] &= ~(1ULL << PHYS_TRACK_BIT(addr));
                ++total_phys;
            }
        }

        curr_item += entry->size + sizeof(uint32_t);
    }

    // TODO: size pretty-printing
    kdebug("%uKiB available\n", total_phys << 2);
}
