#pragma once
#include "sys/types.h"

struct chrdev;

struct tty_data {
    // Process group ID of the foreground group (session leader)
    pid_t fg_pgid;
    // Output device info
    void *out_dev;
    int (*out_putc) (void *dev, char c);
};

// From keyboard handlers and stuff
void tty_control_write(struct chrdev *n, char c);
void tty_data_write(struct chrdev *n, char c);

// From code directly to the screen
void tty_puts(struct chrdev *n, const char *s);
void tty_putc(struct chrdev *n, char c);

void tty_init(void);
