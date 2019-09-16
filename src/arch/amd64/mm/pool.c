#include "arch/amd64/mm/pool.h"
#include "arch/amd64/mm.h"
#include "sys/debug.h"
#include "sys/mem.h"

static struct {
    uint64_t track[512];
    uintptr_t start;
    uintptr_t index_last;
    size_t size;
} amd64_mm_pool;

uint64_t *amd64_mm_pool_alloc(void) {
    uint64_t *r = NULL;

    for (size_t i = amd64_mm_pool.index_last; i < amd64_mm_pool.size >> 18; ++i) {
        for (size_t j = amd64_mm_pool.index_last; j < 64; ++j) {
            if (!(amd64_mm_pool.track[i] & (1ULL << j))) {
                r = (uint64_t *) (amd64_mm_pool.start + ((i << 18) + (j << 12)));
                amd64_mm_pool.track[i] |= (1ULL << j);
                amd64_mm_pool.index_last = i;
                memset(r, 0, 4096);
                return r;
            }
        }
    }

    for (size_t i = 0; i < amd64_mm_pool.index_last; ++i) {
        for (size_t j = amd64_mm_pool.index_last; j < 64; ++j) {
            if (!(amd64_mm_pool.track[i] & (1ULL << j))) {
                r = (uint64_t *) (amd64_mm_pool.start + ((i << 18) + (j << 12)));
                amd64_mm_pool.track[i] |= (1ULL << j);
                amd64_mm_pool.index_last = i;
                memset(r, 0, 4096);
                return r;
            }
        }
    }

    return NULL;
}

void amd64_mm_pool_free(uint64_t *page) {
    uintptr_t a = (uintptr_t) page;

    if (a < amd64_mm_pool.start || a >= (amd64_mm_pool.start + amd64_mm_pool.size)) {
        // TODO: panic
        kfatal("The page does not belong to the pool\n");
        while (1) {
            asm volatile ("cli; hlt");
        }
    }

    a -= amd64_mm_pool.start;

    size_t i = (a >> 18) & 0x1FF;
    size_t j = (a >> 12) & 0x3F;

    if (!(amd64_mm_pool.track[i] & (1ULL << j))) {
        // TODO: panic
        kfatal("Invalid free() of pool page\n");
        while (1) {
            asm volatile ("cli; hlt");
        }
    }

    amd64_mm_pool.track[i] &= ~(1ULL << j);
}

void amd64_mm_pool_init(uintptr_t begin, size_t size) {
    amd64_mm_pool.start = begin;
    amd64_mm_pool.size = size;
    memset(amd64_mm_pool.track, 0, 512 * sizeof(uint64_t));
}
