#pragma once
#include "sys/fs/node.h"

extern vnode_t *tty0;

void tty_buffer_write(int n, char c);
void tty_init(void);
