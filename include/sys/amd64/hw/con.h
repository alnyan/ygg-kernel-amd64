#pragma once

struct winsize;

void amd64_con_get_size(struct winsize *ws);

void con_blink(void);
void amd64_con_putc(int c);
void amd64_con_init(void);
void amd64_con_sync_cursor(void);
