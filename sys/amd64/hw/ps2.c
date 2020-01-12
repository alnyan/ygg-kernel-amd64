#include "sys/amd64/hw/ps2.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/io.h"
#include "sys/debug.h"
#include "sys/tty.h"

static const char ps2_key_table_0[128] = {
    [0x00] = 0,

    [0x01] = '\003',
    [0x02] = '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    [0x0F] = '\t',
    [0x10] = 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    [0x1C] = '\n',
    [0x1E] = 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'',
    [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    [0x39] = ' ',
    [0x7F] = 0
};

uint32_t ps2_irq_keyboard(void *ctx) {
    uint8_t st = inb(0x64);

    if (!(st & 1)) {
        return IRQ_UNHANDLED;
    }

    uint8_t key = inb(0x60);

    if (!(key & 0x80)) {
        char key_char = ps2_key_table_0[key];

        if (key_char != 0) {
            tty_buffer_write(0, key_char);
        }
    }

    return IRQ_HANDLED;
}

void ps2_init(void) {
    irq_add_leg_handler(IRQ_LEG_KEYBOARD, ps2_irq_keyboard, NULL);
}
