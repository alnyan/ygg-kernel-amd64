#pragma once
#include "sys/types.h"
#include "sys/list.h"

struct chrdev;
struct console;
struct console_buffer;

struct tty_data {
    // Process group ID of the foreground group (session leader)
    pid_t fg_pgid;
    // Console to which the TTY is attached
    struct console *master;
    struct console_buffer *buffer;
    // List link of TTYs which share the same console
    struct list_head list;
};

// From keyboard handlers and stuff
void tty_control_write(struct chrdev *n, char c);
void tty_data_write(struct chrdev *n, char c);

// From code directly to the screen
void tty_puts(struct chrdev *n, const char *s);
void tty_putc(struct chrdev *n, char c);

int tty_create(struct console *con);

void tty_init(void);
