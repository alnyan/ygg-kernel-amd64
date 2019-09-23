#include "sys/mem.h"

void *memset(void *blk, int v, size_t sz) {
    for (size_t i = 0; i < sz; ++i) {
        ((char *) blk)[i] = v;
    }
    return blk;
}

void *memcpy(void *dst, const void *src, size_t sz) {
    for (size_t i = 0; i < sz; ++i) {
        ((char *) dst)[i] = ((const char *) src)[i];
    }
    return dst;
}
