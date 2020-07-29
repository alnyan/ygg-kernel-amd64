#pragma once
#include "sys/types.h"
#include "sys/list.h"

struct chrdev;
struct console;
struct console_buffer;

struct tty_serial {
    void *ctx;
    struct chrdev *tty;
    int (*putc) (void *ctx, int ch);
    // TODO: termios configs like baud rate
};

struct tty_data {
    // Process group ID of the foreground group (session leader)
    pid_t fg_pgid;
    int has_console;
    union {
        // Console to which the TTY is attached
        struct console *master;
        struct tty_serial *serial;
    };
    struct console_buffer *buffer;
    // List link of TTYs which share the same console
    struct list_head list;
};

// From keyboard handlers and stuff
void tty_control_write(struct chrdev *n, char c);
void tty_data_write(struct chrdev *n, char c);

// From code directly to the screen
void tty_puts(struct chrdev *n, const char *s);
void tty_putc(struct chrdev *n, int c);

int tty_create(struct console *con);
int tty_create_serial(struct tty_serial *ser);

void tty_init(void);
