#include "arch/amd64/hw/con.h"
#include "sys/display.h"

#define BUFFER      ((uint16_t volatile*) 0xFFFFFF00000B8000)

static struct display display_console = {0};

static void amd64_console_setc(uint16_t y, uint16_t x, uint16_t c) {
    BUFFER[y * 80 + x] = c;
}

void amd64_console_init(void) {
    display_console.flags = 0;
    display_console.width_chars = 80;
    display_console.height_chars = 25;
    display_console.setc = amd64_console_setc;
    list_head_init(&display_console.list);

    display_add(&display_console);
}
