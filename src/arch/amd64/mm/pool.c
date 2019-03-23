#include "pool.h"
#include "sys/mem.h"

static struct {
    uint64_t track[512];
    uintptr_t start;
    size_t size;
} amd64_mm_pool;

void amd64_mm_pool_init(uintptr_t begin, size_t size) {
    amd64_mm_pool.start = begin;
    amd64_mm_pool.size = size;
    memset(amd64_mm_pool.track, 0, 512 * sizeof(uint64_t));
}
