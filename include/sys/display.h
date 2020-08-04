#pragma once
#include "sys/block/blk.h"
#include "sys/types.h"
#include "sys/list.h"

// Not a text mode
#define DISP_GRAPHIC        (1 << 0)
// Has a linear framebuffer
#define DISP_LFB            (1 << 1)
#define DISP_LOCK           (1 << 2)

struct display {
    // Text modes: set character at y, x
    void (*setc) (uint16_t y, uint16_t x, uint16_t c);
    void (*cursor) (uint16_t y, uint16_t x);

    uint32_t flags;
    // If DISP_GRAPHIC
    uint32_t width_pixels, height_pixels;
    uint32_t bpp, pitch;
    // If DISP_LFB
    uintptr_t framebuffer;

    uint16_t width_chars, height_chars;

    // Private data
    void *data;

    // If DISP_LFB
    struct blkdev blk;

    struct list_head list;
};

extern int g_display_blink_state;

void display_add(struct display *d);
struct display *display_create(void);
void display_postinit(void);

void display_setc(struct display *disp, uint16_t y, uint16_t x, uint16_t ch);

struct display *display_get_default(void);
