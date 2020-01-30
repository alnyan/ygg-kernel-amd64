#pragma once
#include "sys/types.h"

struct thread;

void timer_add_sleep(struct thread *thr);
void amd64_timer_init(void);
