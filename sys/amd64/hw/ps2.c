#include "sys/amd64/hw/ps2.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/io.h"
#include "sys/signum.h"
#include "sys/debug.h"
#include "sys/tty.h"

#define PS2_KEY_LSHIFT_DOWN     0x2A
#define PS2_KEY_RSHIFT_DOWN     0x36
#define PS2_KEY_LSHIFT_UP       0xAA
#define PS2_KEY_RSHIFT_UP       0xB6

#define PS2_KEY_LCTRL_DOWN      0x1D
#define PS2_KEY_LCTRL_UP        0x9D

#define PS2_MOD_SHIFT           (1 << 0)
#define PS2_MOD_CTRL            (1 << 1)

static uint32_t ps2_mods = 0;

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

static const char ps2_key_table_1[128] = {
    [0x00] = 0,

    [0x01] = '\003',
    [0x02] = '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    [0x0F] = '\t',
    [0x10] = 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    [0x1C] = '\n',
    [0x1E] = 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"',
    [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
    [0x39] = ' ',
    [0x7F] = 0
};

uint32_t ps2_irq_keyboard(void *ctx) {
    uint8_t st = inb(0x64);

    if (!(st & 1)) {
        return IRQ_UNHANDLED;
    }

    uint8_t key = inb(0x60);

    if (key == PS2_KEY_LSHIFT_DOWN || key == PS2_KEY_RSHIFT_DOWN) {
        ps2_mods |= PS2_MOD_SHIFT;
    }
    if (key == PS2_KEY_LSHIFT_UP || key == PS2_KEY_RSHIFT_UP) {
        ps2_mods &= ~PS2_MOD_SHIFT;
    }
    if (key == PS2_KEY_LCTRL_DOWN) {
        ps2_mods |= PS2_MOD_CTRL;
    }
    if (key == PS2_KEY_LCTRL_UP) {
        ps2_mods &= ~PS2_MOD_CTRL;
    }

    if (!(key & 0x80) && !(ps2_mods & PS2_MOD_CTRL)) {
        // Special keys
        switch (key) {
        case 0x01:
            tty_buffer_write(0, '\033');
            return IRQ_HANDLED;
        case 0x47:
            tty_buffer_write(0, '\033');
            tty_buffer_write(0, '[');
            tty_buffer_write(0, '[');
            tty_buffer_write(0, 'H');
            return IRQ_HANDLED;
        case 0x48:
            tty_buffer_write(0, '\033');
            tty_buffer_write(0, '[');
            tty_buffer_write(0, 'A');
            return IRQ_HANDLED;
        case 0x50:
            tty_buffer_write(0, '\033');
            tty_buffer_write(0, '[');
            tty_buffer_write(0, 'B');
            return IRQ_HANDLED;
        case 0x4D:
            tty_buffer_write(0, '\033');
            tty_buffer_write(0, '[');
            tty_buffer_write(0, 'C');
            return IRQ_HANDLED;
        case 0x4F:
            tty_buffer_write(0, '\033');
            tty_buffer_write(0, '[');
            tty_buffer_write(0, '[');
            tty_buffer_write(0, 'F');
            return IRQ_HANDLED;
        case 0x4B:
            tty_buffer_write(0, '\033');
            tty_buffer_write(0, '[');
            tty_buffer_write(0, 'D');
            return IRQ_HANDLED;
        default:
            break;
        }

        char key_char = ((ps2_mods & PS2_MOD_SHIFT) ? ps2_key_table_1 : ps2_key_table_0)[key];

        if (key_char != 0) {
            tty_buffer_write(0, key_char);
        }
    } else {
        switch (key) {
        case 0x20:  // ^D
            tty_control_write(0, 'd');
            break;
        case 0x2E:  // ^C
            tty_control_write(0, 'c');
            break;
        }
    }

    return IRQ_HANDLED;
}

void ps2_init(void) {
    irq_add_leg_handler(IRQ_LEG_KEYBOARD, ps2_irq_keyboard, NULL);
}
