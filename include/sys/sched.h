#pragma once
#include "sys/thread.h"

void sched_set_cpu_count(size_t ncpus);
void sched_add(struct thread *t);
void sched_init(void);
