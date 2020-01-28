#pragma once
#include "sys/types.h"

extern int sched_ready;

struct thread;

// Terminate all tasks
//void sched_reboot(unsigned int cmd);

//int sched_signal_group(int pgid, int signum);
//int sched_close_stdin_group(int pgid);
//struct thread *sched_find(int pid);

// Used to add a thread to processing queue
// Shall clear thread's "WAITING" flag
void sched_queue(struct thread *thr);
void sched_unqueue(struct thread *thr);

void sched_set_cpu_count(size_t ncpus);
void sched_init(void);
void sched_enter(void);
