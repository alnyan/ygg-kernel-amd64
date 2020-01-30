#pragma once
struct chrdev;

// From keyboard handlers and stuff
void tty_control_write(struct chrdev *n, char c);
void tty_data_write(struct chrdev *n, char c);

// From code directly to the screen
void tty_puts(struct chrdev *n, const char *s);
void tty_putc(struct chrdev *n, char c);

void tty_init(void);
