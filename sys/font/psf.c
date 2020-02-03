#include "sys/font/psf.h"
#include "sys/panic.h"
// XXX: Only supports non-indexed linear buffers

extern char _psf_start;
extern char _psf_end;
static struct psf_font *font;
static uintptr_t psf_fb_addr, psf_fb_pitch;

void psf_init(uintptr_t addr, uintptr_t pitch, uint16_t *charw, uint16_t *charh) {
    font = (struct psf_font *) &_psf_start;
    if (font->magic != PSF_FONT_MAGIC) {
        panic("Invalid PSF magic\n");
    }

    psf_fb_addr = addr;
    psf_fb_pitch = pitch;

    *charw = font->width;
    *charh = font->height;
}

void psf_draw(uint16_t row, uint16_t col, uint8_t c, uint32_t fg, uint32_t bg) {
    if (c >= font->numglyph) {
        c = 0;
    }

    int bytesperline = (font->width + 7) / 8;

    uint8_t *glyph = (uint8_t *) &_psf_start + font->headersize + c * font->bytesperglyph;
    // XXX: Assuming BPP is 32
    /* calculate the upper left corner on screen where we want to display.
       we only do this once, and adjust the offset later. This is faster. */
    uintptr_t offs = (row * font->height * psf_fb_pitch) + (col * font->width * 4);
    /* finally display pixels according to the bitmap */
    uint32_t x, y, line, mask;
    for (y = 0; y < font->height; ++y) {
        /* save the starting position of the line */
        line = offs;
        mask = 1 << (font->width - 1);
        /* display a row */
        for (x = 0; x < font->width; ++x) {
            *((uint32_t *)(psf_fb_addr + line)) = ((int) *glyph) & (mask) ? fg : bg;
            /* adjust to the next pixel */
            mask >>= 1;
            line += 4;
        }
        /* adjust to the next line */
        glyph += bytesperline;
        offs  += psf_fb_pitch;
    }
}
