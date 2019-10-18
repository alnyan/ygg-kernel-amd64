#include "sys/amd64/hw/con.h"
#include "sys/string.h"
#include "sys/types.h"

static uint16_t *con_buffer = (uint16_t *) (0xB8000 + 0xFFFFFF0000000000);
static uint16_t x = 0, y = 0;

static void amd64_con_scroll(void) {
    if (y == 23) {
        for (int i = 0; i < 22; ++i) {
            memcpy(&con_buffer[i * 80], &con_buffer[(i + 1) * 80], 80 * 2);
        }
        memset(&con_buffer[22 * 80], 0, 80 * 2);
        y = 22;
    }
}

void amd64_con_putc(int c) {
    if (c >= ' ') {
        con_buffer[x + y * 80] = c | 0x700;
        ++x;
        if (x >= 80) {
            ++y;
            x = 0;
        }
    } else {
        switch (c) {
        case '\n':
            ++y;
            x = 0;
            break;
        default:
            amd64_con_putc('?');
            return;
        }
    }
    amd64_con_scroll();
}
