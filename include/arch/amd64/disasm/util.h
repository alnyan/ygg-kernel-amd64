#pragma once
#include <stdint.h>

static inline int32_t movsx16(uint16_t src) {
    if (src & 0x8000) {
        return 0xFFFF0000 + src;
    } else {
        return src;
    }
}

static inline int32_t movsx8(uint8_t src) {
    if (src & 0x80) {
        return 0xFFFFFF00 + src;
    } else {
        return src;
    }
}

static inline int64_t qmovsx32(uint32_t src) {
    if (src & 0x80000000) {
        return 0xFFFFFFFF00000000LL + src;
    } else {
        return src;
    }
}

static inline int64_t qmovsx16(uint16_t src) {
    if (src & 0x8000) {
        return 0xFFFFFFFFFFFF0000LL + src;
    } else {
        return src;
    }
}

static inline int64_t qmovsx8(uint8_t src) {
    if (src & 0x80) {
        return 0xFFFFFFFFFFFFFF00LL + src;
    } else {
        return src;
    }
}

static inline int64_t bmask(int64_t src, size_t sz) {
    switch (sz) {
    case 16:
        return src & 0xFFFF;
    default:
        return src;
    }
}
