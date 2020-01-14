#include "sys/amd64/hw/con.h"
#include "sys/string.h"
#include "sys/types.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/panic.h"
#include "sys/amd64/hw/io.h"
#include "sys/psf.h"
#include "sys/mm.h"

#if defined(VESA_ENABLE)
#include "sys/font/logo.h"
#include "sys/amd64/hw/vesa.h"
#else
#define vesa_available 0
#endif

#define ESC_ESC     1
#define ESC_CSI     2

#define CGA_BUFFER_ADDR     0xB8000
#define ATTR_DEFAULT        0x1700ULL

#define ATTR_BOLD           1

static uint16_t *con_buffer;
static uint16_t x = 0, y = 0;
static uint16_t con_width, con_height;
static uint16_t saved_x = 0, saved_y = 0;
static int con_avail = 0;
static uint16_t char_width, char_height;
static int con_blink_state = 0;

// Map CSI colors to CGA
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
static uint32_t esc_argv[8];
static size_t esc_argc;
static char esc_letter = 0;
static int esc_mode = 0;
static uint16_t attr = ATTR_DEFAULT;
static uint16_t xattr = 0;

static void setc(uint16_t row, uint16_t col, uint16_t v);

#if defined(VESA_ENABLE)
void con_blink(void) {
    if (vesa_available) {
        uint32_t fg = rgb_map[(attr >> 8) & 0xF];
        uint32_t bg = rgb_map[(attr >> 12) & 0xF];
        con_blink_state = !con_blink_state;

        if (con_blink_state) {
            psf_draw(y, x, ' ', bg, fg);
        } else {
            psf_draw(y, x, ' ', fg, bg);
        }
    }
}
#endif

void amd64_con_sync_cursor(void) {
    static uint16_t old_pos = 0;
    uint16_t pos = y * 80 + x;

    if (old_pos != pos) {
        // This is slow as hell
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t) (pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
    }
    old_pos = pos;
}

static void amd64_con_flush(void) {
    memcpyq((uint64_t *) MM_VIRTUALIZE(CGA_BUFFER_ADDR), (uint64_t *) con_buffer, 25 * 20);
}

static void amd64_scroll_down(void) {
#if defined(VESA_ENABLE)
    if (vesa_available) {
        for (uint16_t row = 0; row < con_height; ++row) {
            for (uint16_t col = 0; col < con_width; ++col) {
                uint16_t ch = con_buffer[row * con_width + col];
                psf_draw(row, col, ch & 0xFF, rgb_map[(ch >> 8) & 0xF], rgb_map[(ch >> 12) & 0xF]);
            }
        }
    }
#else
    memcpyq((uint64_t *) con_buffer, (uint64_t *) &con_buffer[con_width], 20 * 24);
    memsetq((uint64_t *) &con_buffer[(con_height - 1) * con_width],
            ATTR_DEFAULT | (ATTR_DEFAULT << 16) | (ATTR_DEFAULT << 32) | (ATTR_DEFAULT << 48), 20);
    // Flush whole backbuffer
    amd64_con_flush();
    y = con_height - 1;
#endif
}

static void clear(void) {

#if defined(VESA_ENABLE)
    memsetw(con_buffer, attr, con_width * con_height);
    if (vesa_available) {
        for (size_t i = 0; i < vesa_height; ++i) {
            memsetl((uint32_t *) (vesa_addr + vesa_pitch * i), rgb_map[(attr >> 12) & 0xF], vesa_width);
        }
    }
#else
    memsetq((uint64_t *) con_buffer,
            attr | ((uint64_t) attr << 16) | ((uint64_t) attr << 32) | ((uint64_t) attr << 48), 20 * 25);
    amd64_con_flush();
#endif
}

// I guess I could've implemented VT100 escape handling better, but
// this is all I invented so far
static void process_csi(void) {
    switch (esc_letter) {
    case 'm':
        for (size_t i = 0; i < esc_argc; ++i) {
            uint32_t v = esc_argv[i];
            switch (v / 10) {
            case 0:
                switch (v % 10) {
                case 0:
                    // Reset
                    attr = ATTR_DEFAULT;
                    xattr = 0;
                    break;
                case 1:
                    // Bright
                    xattr |= ATTR_BOLD;
                    break;
                case 2:
                    // Dim
                    xattr &= ~ATTR_BOLD;
                    break;
                case 7:
                    // Reverse
                    attr >>= 4;
                    attr |= (attr & 0xF0) << 8;
                    attr &= 0xFF00;
                    break;
                }
                break;
            case 3:
                // Foreground
                attr &= ~0x0F00;
                attr |= (uint16_t) color_map[v % 10] << 8;
                break;
            case 4:
                // Background
                attr &= ~0xF000;
                attr |= (uint16_t) color_map[v % 10] << 12;
                break;
            }
        }
        break;
    case 'A':
        // Cursor up
        if (!esc_argv[0]) {
            esc_argv[0] = 1;
        }

        if (esc_argv[0] >= y) {
            y = 0;
        } else {
            y -= esc_argv[0];
        }
        break;
    case 'B':
        // Cursor down
        if (!esc_argv[0]) {
            esc_argv[0] = 1;
        }

        if (esc_argv[0] + y >= (uint32_t) (con_height - 1)) {
            y = 22;
        } else {
            y += esc_argv[0];
        }
        break;
    case 'C':
        // Forward
        if (!esc_argv[0]) {
            esc_argv[0] = 1;
        }

        if (esc_argv[0] + x >= con_width) {
            x = 79;
        } else {
            x += esc_argv[0];
        }

        break;
    case 'D':
        // Backward
        if (!esc_argv[0]) {
            esc_argv[0] = 1;
        }

        if (esc_argv[0] >= x) {
            x = 0;
        } else {
            x -= esc_argv[0];
        }

        break;
    case 'K':
        // Erase end of line
        memsetw(&con_buffer[y * con_width + x], attr, con_width - x);
        break;
    case 'J':
        switch (esc_argv[0]) {
        case 0:
            // Erase lines down
            memsetw(con_buffer, attr, con_width * y);
            break;
        case 1:
            // Erase lines up
            memsetw(&con_buffer[y * con_width], attr, con_width * (con_height - y));
            break;
        case 2:
            // Erase all
            clear();
            break;
        }
        break;
    case 'f':
        // Set cursor position
        y = (esc_argv[0] - 1) % con_height;
        x = (esc_argv[1] - 1) % (con_width - 1);
        break;
    case 's':
        // Save cursor
        saved_y = y;
        saved_x = x;
        break;
    case 'u':
        // Unsave cursor
        y = saved_y;
        x = saved_x;
        break;
    }
}

static void setc(uint16_t row, uint16_t col, uint16_t v) {
    if (xattr & ATTR_BOLD) {
        v += 0x800;
    }

    con_buffer[row * con_width + col] = v;

    if (vesa_available) {
        psf_draw(row, col, v & 0xFF, rgb_map[(v >> 8) & 0xF], rgb_map[(v >> 12) & 0xF]);
    } else {
        ((uint16_t *) MM_VIRTUALIZE(CGA_BUFFER_ADDR))[row * 80 + col] = v;
    }
}

void amd64_con_putc(int c) {
    if (!con_avail || !c) {
        // Ignore NULs
        return;
    }

    c = (uint8_t) (c & 0xFF);

    switch (esc_mode) {
    case ESC_CSI:
        if (c >= '0' && c <= '9') {
            esc_argv[esc_argc] *= 10;
            esc_argv[esc_argc] += c - '0';
        } else if (c == ';') {
            esc_argv[++esc_argc] = 0;
        } else {
            ++esc_argc;
            esc_letter = c;
            process_csi();
            esc_mode = 0;
        }
        break;
    case ESC_ESC:
        if (c == '[') {
            esc_mode = ESC_CSI;
            break;
        } else {
            esc_mode = 0;
        }
    __attribute__((fallthrough));
    case 0:
        if (c == '\033') {
            esc_mode = ESC_ESC;
            esc_argv[0] = 0;
            esc_argc = 0;
            return;
        }
        if (c >= ' ') {
            setc(y, x, c | attr);
            ++x;
            if (x >= con_width) {
                ++y;
                x = 0;
                if (y == con_height) {
                    amd64_scroll_down();
                }
            }
        } else {
            switch (c) {
            case '\n':
                ++y;
                x = 0;
                if (y >= con_height) {
                    amd64_scroll_down();
                }
                break;
            default:
                amd64_con_putc('?');
                return;
            }
        }
        break;
    }
}

void amd64_con_init(void) {
#if defined(VESA_ENABLE)
    if (vesa_available) {
        psf_init(vesa_addr, vesa_pitch, &char_width, &char_height);

        con_width = vesa_width / char_width;
        con_height = vesa_height / char_height;
        con_buffer = (uint16_t *) kmalloc(con_width * con_height * 2);
    } else {
#else
    {
#endif
        con_buffer = kmalloc(80 * 25 * 2); //(uint16_t *) MM_VIRTUALIZE(CGA_BUFFER_ADDR);
        con_width = 80;
        con_height = 25;
    }
    con_avail = 1;

    clear();

#if defined(VESA_ENABLE)
    if (vesa_available) {
        y = FONT_LOGO_HEIGHT / char_height;
        char pixel[4];
        char *data = font_logo_header_data;
        uint32_t v;
        for (uint32_t j = 0; j < FONT_LOGO_HEIGHT; ++j) {
            for (uint32_t i = 0; i < FONT_LOGO_WIDTH; ++i) {
                FONT_LOGO_HEADER_PIXEL(data, pixel);
                v = pixel[2] | ((uint32_t) pixel[1] << 8) | ((uint32_t) pixel[0] << 16);
                if (v) {
                    vesa_put(i, j, v);
                }
            }
        }
    }
#endif
}
