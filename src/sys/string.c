#include "string.h"

size_t strlen(const char *a) {
    size_t s = 0;
    while (*a++) {
        ++s;
    }
    return s;
}
