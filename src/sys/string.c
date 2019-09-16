#include "string.h"

size_t strlen(const char *a) {
    size_t s = 0;
    while (*a++) {
        ++s;
    }
    return s;
}

int strncmp(const char *a, const char *b, size_t n) {
    size_t c = 0;
    for (; c < n && (*a || *b); ++c, ++a, ++b) {
        if (*a != *b) {
            return -1;
        }
    }
    return 0;
}
