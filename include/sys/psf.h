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

void psf_init(uintptr_t addr, uintptr_t pitch, uint16_t *charw, uint16_t *charh);
void psf_draw(uint16_t row, uint16_t col, uint8_t c, uint32_t fg, uint32_t bg);
