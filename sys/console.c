#include "sys/char/input.h"
#include "sys/char/tty.h"
#include "sys/char/chr.h"
#include "sys/display.h"
#include "sys/console.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"

#define ATTR_DEFAULT        0x1700

#define ESC_ESC     1
#define ESC_CSI     2

#define ATTR_BOLD   1

static void console_flush(struct console *con, struct console_buffer *buf);

static LIST_HEAD(g_consoles);

struct console_buffer {
    // Current attribute
    uint16_t attr;
    // Extended attributes
    uint16_t xattrs;
    // Escape code processing
    uint32_t esc_argv[8];
    char esc_letter;
    size_t esc_argc;
    int esc_mode;
    // Cursor
    uint16_t y, x;
    // Blink
    uint16_t last_blink_y, last_blink_x;
    uint16_t data[0];
};

static struct console_buffer *console_buffer_create(uint16_t rows, uint16_t cols) {
    struct console_buffer *buf = kmalloc(sizeof(struct console_buffer) + rows * cols * sizeof(uint16_t));
    _assert(buf);
    memsetw(buf->data, ATTR_DEFAULT, rows * cols);
    buf->y = 0;
    buf->x = 0;
    buf->last_blink_x = 0;
    buf->last_blink_y = 0;
    buf->attr = ATTR_DEFAULT;
    buf->xattrs = 0;
    buf->esc_mode = 0;
    return buf;
}

void console_attach(struct console *con, struct chrdev *tty) {
    struct console_buffer *tty_buffer = console_buffer_create(con->width_chars, con->height_chars);
    _assert(tty_buffer);

    struct tty_data *data = tty->dev_data;
    _assert(data);

    // Make sure the TTY hasn't already been enslaved to a console
    _assert(!data->buffer);
    _assert(!data->master);
    data->master = con;
    data->buffer = tty_buffer;

    list_add(&data->list, &con->slaves);

    // Make current active TTY if none yet selected
    if (!con->tty_active) {
        con->tty_active = tty;
        con->buf_active = tty_buffer;

        // Clear new default console
        console_flush(con, tty_buffer);
    }
}

static void console_flush(struct console *con, struct console_buffer *buf) {
    for (uint16_t row = 0; row < con->height_chars; ++row) {
        for (uint16_t col = 0; col < con->width_chars; ++col) {
            uint16_t ch = buf->data[row * con->width_chars + col];
            display_setc(con->display, row, col, ch);
        }
    }
}

static void console_scroll_check(struct console *con, struct console_buffer *buf) {
    if (buf->y >= con->height_chars) {
        buf->y = con->height_chars - 1;

        memcpy(buf->data, &buf->data[con->width_chars], (con->height_chars - 1) * con->width_chars * 2);
        memsetw(&buf->data[(con->height_chars - 1) * con->width_chars], ATTR_DEFAULT, con->width_chars);

        console_flush(con, buf);
    }
}

static uint8_t color_map[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
// I guess I could've implemented VT100 escape handling better, but
// this is all I invented so far
static void process_csi(struct console *con, struct console_buffer *buf) {
    switch (buf->esc_letter) {
    case 'm':
        for (size_t i = 0; i < buf->esc_argc; ++i) {
            uint32_t v = buf->esc_argv[i];
            switch (v / 10) {
            case 0:
                switch (v % 10) {
                case 0:
                    // Reset
                    buf->attr = ATTR_DEFAULT;
                    buf->xattrs = 0;
                    break;
                case 1:
                    // Bright
                    buf->xattrs |= ATTR_BOLD;
                    break;
                case 2:
                    // Dim
                    buf->xattrs &= ~ATTR_BOLD;
                    break;
                case 7:
                    // Reverse
                    buf->attr >>= 4;
                    buf->attr |= (buf->attr & 0xF0) << 8;
                    buf->attr &= 0xFF00;
                    break;
                }
                break;
            case 3:
                // Foreground
                buf->attr &= ~0x0F00;
                buf->attr |= (uint16_t) color_map[v % 10] << 8;
                break;
            case 4:
                // Background
                buf->attr &= ~0xF000;
                buf->attr |= (uint16_t) color_map[v % 10] << 12;
                break;
            }
        }
        break;
    case 'J':
        switch (buf->esc_argv[0]) {
        case 0:
            // Erase lines down
            memsetw(buf->data, buf->attr, con->width_chars * buf->y);
            break;
        case 1:
            // Erase lines up
            memsetw(&buf->data[buf->y * con->width_chars],
                    buf->attr,
                    con->width_chars * (con->height_chars - buf->y));
            break;
        case 2:
            // Erase all
            memsetw(buf->data, buf->attr, con->width_chars * con->height_chars);
            console_flush(con, buf);
            break;
        }
        break;
    case 'f':
        // Set cursor position
        buf->y = (buf->esc_argv[0] - 1) % con->height_chars;
        buf->x = (buf->esc_argv[1] - 1) % (con->width_chars - 1);
        break;
    case 'A':
        // Cursor up
        if (!buf->esc_argv[0]) {
            buf->esc_argv[0] = 1;
        }

        if (buf->esc_argv[0] >= buf->y) {
            buf->y = 0;
        } else {
            buf->y -= buf->esc_argv[0];
        }
        break;
    case 'B':
        // Cursor down
        if (!buf->esc_argv[0]) {
            buf->esc_argv[0] = 1;
        }

        if (buf->esc_argv[0] + buf->y >= (uint32_t) con->height_chars) {
            buf->y = con->height_chars - 1;
        } else {
            buf->y += buf->esc_argv[0];
        }
        break;
    case 'C':
        // Forward
        if (!buf->esc_argv[0]) {
            buf->esc_argv[0] = 1;
        }

        if (buf->esc_argv[0] + buf->x >= con->width_chars) {
            buf->x = con->width_chars - 1;
        } else {
            buf->x += buf->esc_argv[0];
        }

        break;
    case 'D':
        // Backward
        if (!buf->esc_argv[0]) {
            buf->esc_argv[0] = 1;
        }

        if (buf->esc_argv[0] >= buf->x) {
            buf->x = 0;
        } else {
            buf->x -= buf->esc_argv[0];
        }

        break;
    case 'K':
        // Erase end of line
        memsetw(&buf->data[buf->y * con->width_chars + buf->x], buf->attr, con->width_chars - buf->x);
        break;
    default:
        kdebug("\033[31mUnknown CSI sequence: %c\033[0m\n", buf->esc_letter);
        break;
    }
}

static void _console_putc(struct console *con, struct console_buffer *buf, int c) {
    int act = buf == con->buf_active;

    switch (buf->esc_mode) {
    case ESC_CSI:
        if (c >= '0' && c <= '9') {
            buf->esc_argv[buf->esc_argc] *= 10;
            buf->esc_argv[buf->esc_argc] += c - '0';
        } else if (c == ';') {
            buf->esc_argv[++buf->esc_argc] = 0;
        } else {
            ++buf->esc_argc;
            buf->esc_letter = c;
            process_csi(con, buf);
            buf->esc_mode = 0;
        }
        break;
    case ESC_ESC:
        if (c == '[') {
            buf->esc_mode = ESC_CSI;
            break;
        } else {
            buf->esc_mode = 0;
        }
        __attribute__((fallthrough));
    case 0:
        if (c == '\033') {
            buf->esc_mode = ESC_ESC;
            buf->esc_argv[0] = 0;
            buf->esc_argc = 0;

            return;
        }

        if (c == '\n') {
            ++buf->y;
            buf->x = 0;
            console_scroll_check(con, buf);
        } else if (c >= ' ') {
            buf->data[buf->y * con->width_chars + buf->x] = c | buf->attr;
            if (act) {
                uint16_t a = buf->attr;
                if (buf->xattrs & ATTR_BOLD) {
                    a |= 0x8000;
                }
                display_setc(con->display, buf->y, buf->x, c | a);
            }

            ++buf->x;
            if (buf->x >= con->width_chars) {
                ++buf->y;
                buf->x = 0;
                console_scroll_check(con, buf);
            }
        }
    }
}

void console_update_cursor(void) {
    struct console *con;
    static int prev_blink = 0;

    if (g_display_blink_state != prev_blink) {
        list_for_each_entry(con, &g_consoles, list) {
            if (con->display && con->buf_active) {
                // TODO: don't blink on text-mode only consoles:
                //       use hardware cursor
                struct console_buffer *buf = con->buf_active;
                uint16_t c = buf->data[buf->y * con->width_chars + buf->x];
                if (g_display_blink_state) {
                    // Swap attributes
                    c = ((c >> 4) & 0xF00) |
                        ((c & 0xF00) << 4) |
                        (c & 0xFF);
                }
                display_setc(con->display, buf->y, buf->x, c);
                if (buf->last_blink_x != buf->x || buf->last_blink_y != buf->y) {
                    // Also redraw character at last blink position
                    display_setc(con->display,
                                 buf->last_blink_y,
                                 buf->last_blink_x,
                                 buf->data[buf->last_blink_y * con->width_chars +
                                           buf->last_blink_x]);
                    buf->last_blink_x = buf->x;
                    buf->last_blink_y = buf->y;
                }
            }
        }
        prev_blink = g_display_blink_state;
    }
}

void console_putc(struct console *con, struct chrdev *tty, int c) {
    struct tty_data *data = tty->dev_data;
    _assert(data);

    struct console_buffer *buf = data->buffer;
    _assert(buf);

    _console_putc(con, buf, c);
}

void console_type(struct console *con, int c) {
    if (con->tty_active) {
        tty_data_write(con->tty_active, c);
    }
}

void console_default_putc(int c) {
    if (list_empty(&g_consoles)) {
        return;
    }
    struct console *con = list_entry(g_consoles.next, struct console, list);
    if (con->buf_active) {
        _console_putc(con, con->buf_active, c);
    }
}

void console_init_default(void) {
    // Initialize default display+keyboard pair
    struct console *con = kmalloc(sizeof(struct console));
    _assert(con);
    con->tty_active = NULL;
    con->buf_active = NULL;
    list_head_init(&con->slaves);
    list_head_init(&con->list);

    struct display *display = display_get_default();
    _assert(display);
    con->display = display;

    // Extract initial display mode
    con->width_chars = display->width_chars;
    con->height_chars = display->height_chars;
    kdebug("Initializing default console: %ux%u\n", con->width_chars, con->height_chars);

    if (!g_keyboard_console) {
        g_keyboard_console = con;
    }

    // Create a single TTY device
    _assert(tty_create(con) == 0);
    list_add(&con->list, &g_consoles);
}
