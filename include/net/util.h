#pragma once
#include "sys/types.h"

#define FMT_INADDR              "%d.%d.%d.%d"
#define VA_INADDR(v)            (v) >> 24, \
                                ((v) >> 16) & 0xFF, \
                                ((v) >> 8) & 0xFF, \
                                (v) & 0xFF

void strmac(char *dst, const uint8_t *hw);

static inline uint16_t ntohs(uint16_t w) {
    return (w >> 8) | (w << 8);
}
