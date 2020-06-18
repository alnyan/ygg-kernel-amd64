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

static void console_flush(struct console *con, struct console_buffer *buf);

static LIST_HEAD(g_consoles);

struct console_buffer {
    uint16_t attr;
    uint16_t y, x;
    uint16_t data[0];
};

static struct console_buffer *console_buffer_create(uint16_t rows, uint16_t cols) {
    struct console_buffer *buf = kmalloc(sizeof(struct console_buffer) + rows * cols * sizeof(uint16_t));
    _assert(buf);
    memsetw(buf->data, ATTR_DEFAULT, rows * cols);
    buf->y = 0;
    buf->x = 0;
    buf->attr = ATTR_DEFAULT;
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

static void _console_putc(struct console *con, struct console_buffer *buf, int c) {
    int act = buf == con->buf_active;

    if (c == '\n') {
        ++buf->y;
        buf->x = 0;
        console_scroll_check(con, buf);
    } else if (c >= ' ') {
        buf->data[buf->y * con->width_chars + buf->x] = c | buf->attr;
        if (act) {
            display_setc(con->display, buf->y, buf->x, c | buf->attr);
        }

        ++buf->x;
        if (buf->x >= con->width_chars) {
            ++buf->y;
            buf->x = 0;
            console_scroll_check(con, buf);
        }
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
