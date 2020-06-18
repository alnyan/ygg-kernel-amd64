#include "sys/font/psf.h"
#include "sys/display.h"
#include "sys/debug.h"
#include <stddef.h>

static LIST_HEAD(displays);

static uint8_t color_map[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
static uint32_t rgb_map[16] = {
    0x000000,
    0x0000AA,
    0x00AA00,
    0x00AAAA,
    0xAA0000,
    0xAA00AA,
    0xAA5500,
    0xAAAAAA,
    0x555555,
    0x5555FF,
    0x55FF55,
    0x55FFFF,
    0xFF5555,
    0xFF55FF,
    0xFFFF55,
    0xFFFFFF,
};

struct display *display_get_default(void) {
    if (list_empty(&displays)) {
        return NULL;
    }
    return list_entry(displays.next, struct display, list);
}

void display_add(struct display *disp) {
    // Initialize text mode for framebuffers
    if (disp->flags & DISP_GRAPHIC) {
        disp->width_chars = disp->width_pixels / font->width;
        disp->height_chars = disp->height_pixels / font->height;
    }

    list_add(&disp->list, &displays);
}

void display_setc(struct display *disp, uint16_t y, uint16_t x, uint16_t ch) {
    if (disp->flags & DISP_GRAPHIC) {
        // TODO: attr conversion
        psf_draw(disp, y, x, ch & 0xFF, rgb_map[(ch >> 8) & 0xF], rgb_map[(ch >> 12) & 0xF]);
    }
}
