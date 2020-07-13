#include "arch/amd64/context.h"
#include "arch/amd64/mm/pool.h"
#include "arch/amd64/mm/phys.h"
#include "arch/amd64/hw/irq.h"
#include "arch/amd64/hw/idt.h"
#include "arch/amd64/cpu.h"
#include "user/signum.h"
#include "user/reboot.h"
#include "sys/reboot.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/spin.h"
#include "sys/mm.h"

void yield(void);

//// Thread queueing

static struct thread *queue_heads[AMD64_MAX_SMP] = {NULL};
static struct thread threads_idle[AMD64_MAX_SMP] = {0};
static size_t queue_sizes[AMD64_MAX_SMP] = {0};
int sched_ncpus = 1;
int sched_ready = 0;

static spin_t sched_lock = 0;

void sched_set_ncpus(int ncpus) {
    kinfo("Setting ncpus to %d\n", ncpus);
    sched_ncpus = ncpus;
}

static void *idle(void *arg) {
    while (1) {
        asm volatile ("hlt");
    }
    return 0;
}

////

void sched_queue_to(struct thread *thr, int cpu_no) {
    // FIXME: if this happens, there's absolutely some logic error -
    //        I should find out the places where threads are queued
    //        twice
    if (thr->sched_prev || thr->sched_next) {
        return;
    }
    uintptr_t irq;
    spin_lock_irqsave(&sched_lock, &irq);
    _assert(thr);
    thr->state = THREAD_READY;
    thr->cpu = cpu_no;

    if (queue_heads[cpu_no]) {
        struct thread *queue_tail = queue_heads[cpu_no]->sched_prev;

        queue_tail->sched_next = thr;
        thr->sched_prev = queue_tail;
        queue_heads[cpu_no]->sched_prev = thr;
        thr->sched_next = queue_heads[cpu_no];
    } else {
        thr->sched_next = thr;
        thr->sched_prev = thr;

        queue_heads[cpu_no] = thr;
    }

    ++queue_sizes[cpu_no];
    spin_release_irqrestore(&sched_lock, &irq);
}

void sched_queue(struct thread *thr) {
    if (thr->proc && thr->proc->proc_state == PROC_SUSPENDED) {
        panic("Tried to queue a thread from suspended process\n");
    }
#if defined(AMD64_SMP)
    size_t min_queue_size = (size_t) -1;
    int min_queue_index = 0;
    uintptr_t irq;
    spin_lock_irqsave(&sched_lock, &irq);

    for (int i = 0; i < sched_ncpus; ++i) {
        if (queue_sizes[i] < min_queue_size) {
            min_queue_index = i;
            min_queue_size = queue_sizes[i];
        }
    }
    spin_release_irqrestore(&sched_lock, &irq);
    sched_queue_to(thr, min_queue_index);
#else
    sched_queue_to(thr, 0);
#endif
}

void sched_unqueue(struct thread *thr, enum thread_state new_state) {
    uintptr_t irq;
    spin_lock_irqsave(&sched_lock, &irq);

    struct cpu *cpu = get_cpu();
#if defined(AMD64_SMP)
    assert(thr->cpu >= 0, "Tried to unqueue non-queued thread\n");
    if ((int) cpu->processor_id != thr->cpu) {
        // Need to ask another CPU to unqueue the task
        panic("TODO: implement cross-CPU unqueue\n");
    }
#endif
    thr->cpu = -1;
    int cpu_no = cpu->processor_id;

    struct thread *sched_prev = thr->sched_prev;
    struct thread *sched_next = thr->sched_next;

    _assert((new_state == THREAD_WAITING) ||
            (new_state == THREAD_STOPPED));
    _assert(queue_sizes[cpu_no]);
    --queue_sizes[cpu_no];
    thr->state = new_state;

    spin_release_irqrestore(&sched_lock, &irq);
    thread_check_signal(thr, 0);
    spin_lock_irqsave(&sched_lock, &irq);

    if (sched_next == thr) {
        thr->sched_next = NULL;
        thr->sched_prev = NULL;

        queue_heads[cpu_no] = NULL;

        spin_release_irqrestore(&sched_lock, &irq);

        cpu->thread = &threads_idle[cpu_no];
        context_switch_to(&threads_idle[cpu_no], thr);
        return;
    }

    if (thr == queue_heads[cpu_no]) {
        queue_heads[cpu_no] = sched_next;
    }

    _assert(thr && sched_next && sched_prev);

    sched_next->sched_prev = sched_prev;
    sched_prev->sched_next = sched_next;

    thr->sched_prev = NULL;
    thr->sched_next = NULL;

    spin_release_irqrestore(&sched_lock, &irq);
    if (thr == cpu->thread) {
        cpu->thread = sched_next;
        context_switch_to(sched_next, thr);
    }
}

//#if defined(DEBUG_COUNTERS)
//static void sched_debug_tree(int level, struct thread *thr, int depth) {
//    for (int i = 0; i < depth; ++i) {
//        debugs(level, "  ");
//    }
//
//    if (thr->name[0]) {
//        debugf(level, "%s (", thr->name);
//    }
//    debugf(level, "%d @ %d", thr->pid, thr->cpu);
//    if (thr->name[0]) {
//        debugc(level, ')');
//    }
//    debugc(level, ' ');
//
//    switch (thr->state) {
//    case THREAD_RUNNING:
//        debugs(level, "RUN ");
//        break;
//    case THREAD_READY:
//        debugs(level, "RDY ");
//        break;
//    case THREAD_WAITING:
//        debugs(level, "WAIT");
//        break;
//    case THREAD_STOPPED:
//        debugs(level, "STOP");
//        break;
//    default:
//        debugs(level, "UNKN");
//        break;
//    }
//
//    if (thr->first_child) {
//        debugs(level, " {\n");
//
//        for (struct thread *child = thr->first_child; child; child = child->next_child) {
//            sched_debug_tree(level, child, depth + 1);
//        }
//
//        for (int i = 0; i < depth; ++i) {
//            debugs(level, "  ");
//        }
//        debugs(level, "}\n");
//    } else {
//        debugc(level, '\n');
//    }
//}
//
//void sched_debug_cycle(uint64_t delta_ms) {
//    extern struct thread user_init;
//
//    struct heap_stat st;
//    struct amd64_phys_stat phys_st;
//    struct amd64_pool_stat pool_st;
//
//    for (int cpu = 0; cpu < sched_ncpus; ++cpu) {
//        debugf(DEBUG_DEFAULT, "cpu%d: ", cpu);
//
//        struct thread *head = queue_heads[cpu];
//        if (head) {
//            struct thread *tail = head->sched_prev;
//
//            do {
//                if (head->name[0]) {
//                    debugf(DEBUG_DEFAULT, "%s (", head->name);
//                }
//                debugf(DEBUG_DEFAULT, "%d", head->pid);
//                if (head->name[0]) {
//                    debugc(DEBUG_DEFAULT, ')');
//                }
//
//                if (head == tail) {
//                    break;
//                } else {
//                    debugs(DEBUG_DEFAULT, ", ");
//                }
//                head = head->sched_next;
//            } while (1);
//
//            debugs(DEBUG_DEFAULT, "\n");
//        } else {
//            debugs(DEBUG_DEFAULT, "[idle]\n");
//        }
//    }
//
//    kdebug("--- DEBUG_CYCLE ---\n");
//
//    debugs(DEBUG_DEFAULT, "Process tree:\n");
//    sched_debug_tree(DEBUG_DEFAULT, &user_init, 0);
//
//    heap_stat(heap_global, &st);
//    kdebug("Heap stat:\n");
//    kdebug("Allocated blocks: %u\n", st.alloc_count);
//    kdebug("Used %S, free %S, total %S\n", st.alloc_size, st.free_size, st.total_size);
//
//    amd64_phys_stat(&phys_st);
//    kdebug("Physical memory:\n");
//    kdebug("Used %S (%u), free %S (%u), ceiling %p\n",
//            phys_st.pages_used * 0x1000,
//            phys_st.pages_used,
//            phys_st.pages_free * 0x1000,
//            phys_st.pages_free,
//            phys_st.limit);
//
//    amd64_mm_pool_stat(&pool_st);
//    kdebug("Paging pool:\n");
//    kdebug("Used %S (%u), free %S (%u)\n",
//            pool_st.pages_used * 0x1000,
//            pool_st.pages_used,
//            pool_st.pages_free * 0x1000,
//            pool_st.pages_free);
//
//    kdebug("--- ----------- ---\n");
//}
//#endif
//
void yield(void) {
    uintptr_t irq;
    spin_lock_irqsave(&sched_lock, &irq);
    struct cpu *cpu = get_cpu();
    struct thread *from = cpu->thread;
    struct thread *to;

    if (from && from->sched_next) {
        to = from->sched_next;
    } else if (queue_heads[cpu->processor_id]) {
        to = queue_heads[cpu->processor_id];
    } else {
        to = &threads_idle[cpu->processor_id];
    }

    spin_release_irqrestore(&sched_lock, &irq);

    // Check if instead of switching to a proper thread context we
    // have to use signal handling
    thread_check_signal(from, 0);

    if (from) {
        from->state = THREAD_READY;
    }

    _assert(to->state != THREAD_STOPPED);
    to->state = THREAD_RUNNING;
    cpu->thread = to;

    context_switch_to(to, from);
}

void sched_reboot(unsigned int cmd) {
    struct process *user_init = process_find(1);
    _assert(user_init);
    _assert(user_init->proc_state != PROC_FINISHED);

    // TODO: maybe send signal to all the programs
    process_signal(user_init, SIGTERM);

    while (user_init->proc_state != PROC_FINISHED) {
        yield();
    }

    system_power_cmd(cmd);
}

void sched_init(void) {
    for (int i = 0; i < sched_ncpus; ++i) {
        thread_init(&threads_idle[i], (uintptr_t) idle, 0, 0);
        threads_idle[i].cpu = i;
        threads_idle[i].proc = NULL;
    }

    sched_ready = 1;
}

void sched_enter(void) {
    struct cpu *cpu = get_cpu();
    kinfo("cpu%u entering sched\n", cpu->processor_id);

    extern void amd64_irq0(void);
    amd64_idt_set(cpu->processor_id, 32, (uintptr_t) amd64_irq0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    struct thread *first_task = queue_heads[cpu->processor_id];
    if (!first_task) {
        first_task = &threads_idle[cpu->processor_id];
    }

    first_task->state = THREAD_RUNNING;
    cpu->thread = first_task;
    context_switch_first(first_task);
}
