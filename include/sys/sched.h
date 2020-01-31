#pragma once
enum thread_state;
struct thread;

// Enter a newly-created task
extern void context_enter(struct thread *thr);
// Enter a task from exec
extern void context_exec_enter(void *arg, struct thread *thr, uintptr_t stack3, uintptr_t entry);
// Stores current task context, loads new one's
extern void context_switch_to(struct thread *thr, struct thread *from);
// No current task, only load the first task to begin execution
extern void context_switch_first(struct thread *thr);

void sched_queue(struct thread *thr);
void sched_unqueue(struct thread *thr, enum thread_state new_state);

void sched_debug_cycle(uint64_t delta_ms);

void sched_init(void);
void sched_enter(void);
