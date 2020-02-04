#include "arch/amd64/mm/pool.h"
#include "arch/amd64/mm/mm.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/string.h"
#include "sys/spin.h"

static struct {
    uint64_t track[512];
    uintptr_t start;
    uintptr_t index_last;
    size_t size;
} amd64_mm_pool;

static spin_t pool_lock = 0;

void amd64_mm_pool_stat(struct amd64_pool_stat *st) {
    uintptr_t irq;
    spin_lock_irqsave(&pool_lock, &irq);

    st->pages_free = 0;
    st->pages_used = 0;

    for (size_t i = 0; i < amd64_mm_pool.size >> 18; ++i) {
        for (size_t j = 0; j < 64; ++j) {
            if (amd64_mm_pool.track[i] & (1ULL << j)) {
                ++st->pages_used;
            } else {
                ++st->pages_free;
            }
        }
    }

    spin_release_irqrestore(&pool_lock, &irq);
}

uint64_t *amd64_mm_pool_alloc(void) {
    uintptr_t irq;
    spin_lock_irqsave(&pool_lock, &irq);
    uint64_t *r = NULL;

    for (size_t i = amd64_mm_pool.index_last; i < amd64_mm_pool.size >> 18; ++i) {
        for (size_t j = 0; j < 64; ++j) {
            if (!(amd64_mm_pool.track[i] & (1ULL << j))) {
                r = (uint64_t *) (amd64_mm_pool.start + ((i << 18) + (j << 12)));
                amd64_mm_pool.track[i] |= (1ULL << j);
                amd64_mm_pool.index_last = i;
                memset(r, 0, 4096);
                spin_release_irqrestore(&pool_lock, &irq);
                return r;
            }
        }
    }

    for (size_t i = 0; i < amd64_mm_pool.index_last; ++i) {
        for (size_t j = 0; j < 64; ++j) {
            if (!(amd64_mm_pool.track[i] & (1ULL << j))) {
                r = (uint64_t *) (amd64_mm_pool.start + ((i << 18) + (j << 12)));
                amd64_mm_pool.track[i] |= (1ULL << j);
                amd64_mm_pool.index_last = i;
                memset(r, 0, 4096);
                spin_release_irqrestore(&pool_lock, &irq);
                return r;
            }
        }
    }

    spin_release_irqrestore(&pool_lock, &irq);
    return NULL;
}

void amd64_mm_pool_free(uint64_t *page) {
    uintptr_t irq;
    spin_lock_irqsave(&pool_lock, &irq);
    uintptr_t a = (uintptr_t) page;

    if (a < amd64_mm_pool.start || a >= (amd64_mm_pool.start + amd64_mm_pool.size)) {
        panic("The page does not belong to the pool: %p\n", a);
    }

    a -= amd64_mm_pool.start;

    size_t i = (a >> 18) & 0x1FF;
    size_t j = (a >> 12) & 0x3F;

    assert(amd64_mm_pool.track[i] & (1ULL << j), "Double free error (pool): %p\n", page);

    amd64_mm_pool.track[i] &= ~(1ULL << j);
    spin_release_irqrestore(&pool_lock, &irq);
}

void amd64_mm_pool_init(uintptr_t begin, size_t size) {
    amd64_mm_pool.start = begin;
    amd64_mm_pool.size = size;
    memset(amd64_mm_pool.track, 0, 512 * sizeof(uint64_t));
}
