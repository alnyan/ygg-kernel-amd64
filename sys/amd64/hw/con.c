#include "sys/amd64/hw/con.h"
#include "sys/string.h"
#include "sys/types.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/amd64/hw/io.h"

#define ESC_ESC     1
#define ESC_CSI     2

static uint16_t *con_buffer = (uint16_t *) (0xB8000 + 0xFFFFFF0000000000);
static uint16_t x = 0, y = 0;
static uint16_t saved_x = 0, saved_y = 0;

// Map CSI colors to CGA
static uint8_t color_map[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
static uint32_t esc_argv[8];
static size_t esc_argc;
static char esc_letter = 0;
static int esc_mode = 0;
static uint16_t attr = 0x700;

static void amd64_con_moveto(uint8_t row, uint8_t col) {
    uint16_t pos = row * 80 + col;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

static void amd64_con_scroll(void) {
    if (y == 23) {
        for (int i = 0; i < 22; ++i) {
            memcpy(&con_buffer[i * 80], &con_buffer[(i + 1) * 80], 80 * 2);
        }
        memset(&con_buffer[22 * 80], 0, 80 * 2);
        y = 22;
    }
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
                    attr = 0x700;
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
        amd64_con_moveto(y, x);
        break;
    case 'B':
        // Cursor down
        if (!esc_argv[0]) {
            esc_argv[0] = 1;
        }

        if (esc_argv[0] + y >= 23) {
            y = 22;
        } else {
            y += esc_argv[0];
        }
        amd64_con_moveto(y, x);
        break;
    case 'C':
        // Forward
        if (!esc_argv[0]) {
            esc_argv[0] = 1;
        }

        if (esc_argv[0] + x >= 80) {
            x = 79;
        } else {
            x += esc_argv[0];
        }

        amd64_con_moveto(y, x);
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

        amd64_con_moveto(y, x);
        break;
    case 'f':
        // Set cursor position
        y = (esc_argv[0] - 1) % 80;
        x = (esc_argv[1] - 1) % 23;
        amd64_con_moveto(y, x);
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

void amd64_con_putc(int c) {
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
            con_buffer[x + y * 80] = c | attr;
            ++x;
            if (x >= 80) {
                ++y;
                x = 0;
            }
        } else {
            switch (c) {
            case '\n':
                ++y;
                x = 0;
                break;
            default:
                amd64_con_putc('?');
                return;
            }
        }
        amd64_con_moveto(y, x);
        amd64_con_scroll();
        break;
    }
}
