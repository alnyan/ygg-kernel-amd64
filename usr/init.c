#include <stdint.h>

void _start(void) {
    while (1) {
        // XXX: I'm still allowed to do so (for testing, of course)
        *((uint16_t *) 0xFFFFFF00000B8000) = 'A' | 0x900;
    }
}
