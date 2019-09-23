// For now, the timer driver only has support for 8253 PIT
#pragma once
#include <stdint.h>

extern uint64_t amd64_timer_ticks;
extern void (*amd64_timer_tick)(void);

void amd64_timer_configure(void);
