#pragma once
#include "sys/thread.h"

struct thread *sched_find(int pid);
void sched_set_cpu_count(size_t ncpus);
void sched_add(struct thread *t);
void sched_init(void);
