#pragma once
#include "sys/fs/node.h"

void tty_control_write(int n, char c);
void tty_buffer_write(int n, char c);
void tty_init(void);
