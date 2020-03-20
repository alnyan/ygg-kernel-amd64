#include "net/util.h"

void strmac(char *dst, const uint8_t *hw) {
    static const char hex_digits[] = "0123456789abcdef";
    // I don't have snprintf, lol
    for (size_t i = 0; i < 6; ++i) {
        dst[i * 3 + 0] = hex_digits[hw[i] >> 4];
        dst[i * 3 + 1] = hex_digits[hw[i] & 0xF];
        dst[i * 3 + 2] = i == 5 ? 0 : ':';
    }
}
