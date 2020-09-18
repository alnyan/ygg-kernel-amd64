#pragma once
#include "sys/types.h"
#include "sys/list.h"

struct chrdev;
struct display;
struct console_buffer;

struct console {
    struct display *display;

    // Current display
    struct chrdev *tty_active;
    struct console_buffer *buf_active;

    // This console's slaves
    struct list_head slaves;
    // All consoles
    struct list_head list;

    uint16_t width_chars, height_chars;
};

// Enslave a TTY to a physical console
void console_attach(struct console *con, struct chrdev *tty);

uint16_t console_buffer_at(uint16_t y, uint16_t x);
void console_resize(struct console *con, uint16_t new_width, uint16_t new_height);
void console_update_cursor(void);

void console_putc(struct console *con, struct chrdev *tty, int c);
void console_type(struct console *con, int c);

void console_default_putc(int c);

struct console *console_get_default(void);

void console_init_early(struct display *output);
void console_init_default(void);
