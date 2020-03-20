#pragma once
#include "sys/types.h"

#define FMT_INADDR              "%d.%d.%d.%d"
#define VA_INADDR(v)            (v) >> 24, \
                                ((v) >> 16) & 0xFF, \
                                ((v) >> 8) & 0xFF, \
                                (v) & 0xFF

void strmac(char *dst, const uint8_t *hw);

static inline uint32_t ntohl(uint32_t w) {
    uint8_t *s = (uint8_t *)&w;
    return (uint32_t) (s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
}

static inline uint16_t ntohs(uint16_t w) {
    return (w >> 8) | (w << 8);
}
static inline uint16_t htons(uint16_t w) {
    return (w >> 8) | (w << 8);
}
