#pragma once
#include "sys/types.h"

#define PSF_FONT_MAGIC 0x864ab572

struct psf_font {
    uint32_t magic;
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
};

extern struct psf_font *font;

struct display;
void psf_draw(struct display *disp, uint16_t row, uint16_t col, uint8_t c, uint32_t fg, uint32_t bg);
