#include "sys/tty.h"
#include "sys/amd64/hw/io.h"
#include "sys/debug.h"

static char _ps2_table0[] = {
    [0x00] = 0, [0x01] = '\x033',
    [0x02] = '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    [0x0C] = '-', '=', '\b', '\t',
    [0x10] = 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    [0x1A] = '[', ']', '\n', 0,
    [0x1E] = 'a', 's', 'd', 'f', 'h', 'j', 'k', 'l',
    [0x27] = ';', '\'', '`', 0, '\\',
    [0x2C] = 'z', 'x', 'c', 'v', 'b', 'n', 'm',
    [0x33] = ',', '.', '/', 0, '*', 0, ' ', 0,
    [0xFF] = 0
};

void amd64_irq1(void) {
    // Pop event from keyboard buffer
    uint8_t ev = inb(0x60);

    if (ev < 0x80) {
        char sym = _ps2_table0[ev];

        if (sym != 0) {
            tty_buffer_write(0, sym);
        }
    }
}
