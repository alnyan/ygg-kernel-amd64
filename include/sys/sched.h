#pragma once
#include "sys/thread.h"

// Terminate all tasks
void sched_reboot(unsigned int cmd);

int sched_signal_group(int pgid, int signum);
struct thread *sched_find(int pid);
void sched_set_cpu_count(size_t ncpus);
void sched_add(struct thread *t);
void sched_init(void);
