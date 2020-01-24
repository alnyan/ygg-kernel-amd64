#include "sys/amd64/hw/ps2.h"
#include "sys/panic.h"
#include "sys/amd64/hw/irq.h"
#include "sys/input.h"
#include "sys/amd64/hw/io.h"
#include "sys/ctype.h"
#include "sys/signum.h"
#include "sys/debug.h"
#include "sys/line.h"
#include "sys/tty.h"
#include "sys/chr.h"
#include "sys/dev.h"

// Keyboard stuff
#define PS2_KEY_LSHIFT_DOWN     0x2A
#define PS2_KEY_RSHIFT_DOWN     0x36
#define PS2_KEY_LSHIFT_UP       0xAA
#define PS2_KEY_RSHIFT_UP       0xB6

#define PS2_KEY_LCTRL_DOWN      0x1D
#define PS2_KEY_LCTRL_UP        0x9D

#define PS2_KEY_CAPS_LOCK_DOWN  0x3A

// Mouse stuff
#define PS2_AUX_BM              (1 << 2)
#define PS2_AUX_BR              (1 << 1)
#define PS2_AUX_BL              (1 << 0)

// Controller stuff
// ...

static uint32_t ps2_kbd_mods = 0;
static uint32_t ps2_aux_state = 0;

static struct chrdev _ps2_mouse = {
    .dev_data = NULL,
    .write = NULL,
    .read = simple_line_read,
    .ioctl = NULL
};

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

    if (key == 0xE0) {
        kinfo("0xE0 key\n");
    }

    if (key == PS2_KEY_LSHIFT_DOWN || key == PS2_KEY_RSHIFT_DOWN) {
        ps2_kbd_mods |= INPUT_MOD_SHIFT;
    }
    if (key == PS2_KEY_LSHIFT_UP || key == PS2_KEY_RSHIFT_UP) {
        ps2_kbd_mods &= ~INPUT_MOD_SHIFT;
    }
    if (key == PS2_KEY_LCTRL_DOWN) {
        ps2_kbd_mods |= INPUT_MOD_CONTROL;
    }
    if (key == PS2_KEY_LCTRL_UP) {
        ps2_kbd_mods &= ~INPUT_MOD_CONTROL;
    }

    if (key == PS2_KEY_CAPS_LOCK_DOWN) {
        ps2_kbd_mods ^= INPUT_MOD_CAPS;
    }

    if (!g_keyboard_tty) {
        // No TTY to send strokes to
        return IRQ_HANDLED;
    }

    if (!(key & 0x80)) {
        input_key(ps2_kbd_mods, key, ps2_key_table_0, ps2_key_table_1);
    }

    return IRQ_HANDLED;
}

uint32_t ps2_irq_mouse(void *arg) {
    uint8_t packet[4];

    if (!(inb(0x64) & 1)) {
        return IRQ_UNHANDLED;
    }

    for (int i = 0; i < 3; ++i) {
        packet[i] = inb(0x60);
    }

    // TODO: ability to "hold" and "release" the buffer
    //       so objects are written at least in one piece

    int16_t dx, dy;
    dx = packet[1];
    dx -= (packet[0] << 4) & 0x100;

    dy = packet[2];
    dy -= (packet[0] << 3) & 0x100;

    if (dx || dy) {
        // Write "delta" event
        ring_putc(NULL, &_ps2_mouse.buffer, 'd', 0);
        ring_write(NULL, &_ps2_mouse.buffer, &dx, 2, 0);
        ring_write(NULL, &_ps2_mouse.buffer, &dy, 2, 0);
    }

    ps2_aux_state = packet[0] & 7;

    return IRQ_HANDLED;
}

static uint8_t ps2_aux_read_cmd(uint8_t cmd) {
    outb(0x64, 0xD4);
    outb(0x60, cmd);
    while (!(inb(0x64) & 1)) asm ("pause");
    return inb(0x60);
}

void ps2_init(void) {
    // Enable data reporting
    ps2_aux_read_cmd(0xF4);

    outb(0x64, 0x20);
    while (!(inb(0x64) & 1)) asm ("pause");
    uint8_t b = inb(0x60);
    b |= (1 << 1) | (1 << 0);
    b &= ~((1 << 5) | (1 << 4));
    outb(0x64, 0x60);
    outb(0x60, b);

    outb(0x64, 0xA8);

    irq_add_leg_handler(IRQ_LEG_KEYBOARD, ps2_irq_keyboard, NULL);
    irq_add_leg_handler(12, ps2_irq_mouse, NULL);
}

void ps2_register_device(void) {
    ring_init(&_ps2_mouse.buffer, 16);
    dev_add(DEV_CLASS_CHAR, 0, &_ps2_mouse, "ps2aux");
}
