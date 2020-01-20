#pragma once
#include "sys/fs/node.h"

void tty_signal_write(int n, int signum);
void tty_buffer_write(int n, char c);
void tty_init(void);
