#include "arch/amd64/hw/con.h"
#include "arch/amd64/hw/io.h"
#include "sys/display.h"

#define BUFFER      ((uint16_t volatile*) 0xFFFFFF00000B8000)

static struct display display_console = {0};

static void amd64_console_setc(uint16_t y, uint16_t x, uint16_t c) {
    BUFFER[y * 80 + x] = c;
}

static void amd64_console_cursor(uint16_t y, uint16_t x) {
    uint16_t pos = y * 80 + x;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void amd64_console_init(void) {
    display_console.flags = 0;
    display_console.width_chars = 80;
    display_console.height_chars = 25;
    display_console.setc = amd64_console_setc;
    display_console.cursor = amd64_console_cursor;
    list_head_init(&display_console.list);

    display_add(&display_console);
}
