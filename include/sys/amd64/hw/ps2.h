#pragma once
struct chrdev;

void ps2_init(void);
void ps2_register_device(void);
void ps2_kbd_set_tty(struct chrdev *dev);
