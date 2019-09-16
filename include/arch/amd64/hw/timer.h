// For now, the timer driver only has support for 8253 PIT
#pragma once
#include <stdint.h>

extern uint64_t amd64_timer_ticks;

void amd64_timer_configure(void);
