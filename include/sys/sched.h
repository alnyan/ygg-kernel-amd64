#pragma once
enum thread_state;
struct thread;

extern int sched_ncpus;
extern int sched_ready;

void sched_queue(struct thread *thr);
void sched_unqueue(struct thread *thr, enum thread_state new_state);

void sched_set_ncpus(int ncpus);

void sched_debug_cycle(uint64_t delta_ms);
void sched_reboot(unsigned int cmd);

void yield(void);

void sched_init(void);
void sched_enter(void);
