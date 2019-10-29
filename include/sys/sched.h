#pragma once
#include "sys/thread.h"

void sched_add(struct thread *t);
void sched_init(void);
