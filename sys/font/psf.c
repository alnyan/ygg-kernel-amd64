#include "sys/font/psf.h"
#include "sys/display.h"
#include "sys/panic.h"
#include "sys/attr.h"

extern char _psf_start;
extern char _psf_end;
struct psf_font *font;

static __init void psf_init(void) {
    font = (struct psf_font *) &_psf_start;
    if (font->magic != PSF_FONT_MAGIC) {
        panic("Invalid PSF magic\n");
    }
}

void psf_draw(struct display *disp, uint16_t row, uint16_t col, uint8_t c, uint32_t fg, uint32_t bg) {
    if (c >= font->numglyph) {
        c = 0;
    }

    int bytesperline = (font->width + 7) / 8;

    uint8_t *glyph = (uint8_t *) &_psf_start + font->headersize + c * font->bytesperglyph;
    // XXX: Assuming BPP is 32
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */
    uintptr_t offs = (row * font->height * disp->pitch) + (col * font->width * 4);
    /* finally display pixels according to the bitmap */
    uint32_t x, y, line, mask;
    for (y = 0; y < font->height; ++y) {
        /* save the starting position of the line */
        line = offs;
        mask = 1 << (font->width - 1);
        /* display a row */
        for (x = 0; x < font->width; ++x) {
            *((uint32_t *)(disp->framebuffer + line)) = ((int) *glyph) & (mask) ? fg : bg;
            /* adjust to the next pixel */
            mask >>= 1;
            line += 4;
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += disp->pitch;
    }
}
