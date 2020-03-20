#pragma once
#include "sys/types.h"

void strmac(char *dst, const uint8_t *hw);

static inline uint16_t ntohs(uint16_t w) {
    return (w >> 8) | (w << 8);
}
