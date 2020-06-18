#include "arch/amd64/hw/ioapic.h"
#include "arch/amd64/hw/timer.h"
#include "arch/amd64/hw/vesa.h"
#include "arch/amd64/hw/apic.h"
#include "arch/amd64/hw/irq.h"
#include "arch/amd64/hw/con.h"
#include "arch/amd64/hw/idt.h"
#include "arch/amd64/hw/io.h"
#include "arch/amd64/cpu.h"
#include "user/time.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "sys/spin.h"

#define TIMER_PIT                   1

#define PIT_FREQ_BASE               1193182
// Gives ~1kHz, ~1ms resolution
#define PIT_DIV                     1193
#define PIT_CH0                     0x40
#define PIT_CMD                     0x43

uint64_t int_timer_ticks = 0;

static spin_t g_sleep_lock = 0;
static LIST_HEAD(g_sleep_head);

void timer_add_sleep(struct thread *thr) {
    //if (thr->pid > 0)
        //kdebug("Adding %d to sleepers\n", thr->pid);
    uintptr_t irq;
    struct io_notify *n = &thr->sleep_notify;
    spin_lock_irqsave(&g_sleep_lock, &irq);
    list_add(&n->link, &g_sleep_head);
    spin_release_irqrestore(&g_sleep_lock, &irq);
}

void timer_remove_sleep(struct thread *thr) {
    //if (thr->pid > 0)
        //kdebug("Removing sleep %d\n", thr->pid);
    uintptr_t irq;
    struct io_notify *n = &thr->sleep_notify;
    struct io_notify *it;
    spin_lock_irqsave(&g_sleep_lock, &irq);
    list_for_each_entry(it, &g_sleep_head, link) {
        if (it == n) {
            list_del_init(&it->link);
            spin_release_irqrestore(&g_sleep_lock, &irq);
            return;
        }
    }
    spin_release_irqrestore(&g_sleep_lock, &irq);
    panic("No such thread\n");
}

static uint32_t timer_tick(void *arg) {
    switch ((uint64_t) arg) {
    case TIMER_PIT:
//        #if defined(VESA_ENABLE)
//        ++int_timer_ticks;
//        if (int_timer_ticks >= 300) {
//            con_blink();
//            int_timer_ticks = 0;
//        }
//        if (!vesa_available) {
//#else
//        {
//#endif
//            amd64_con_sync_cursor();
//        }
        // Each tick is approx. 1ms, so add 1ms to system time
        system_time += 1000000;
        break;
    }

#if defined(DEBUG_COUNTERS)
    static uint64_t last_debug_cycle = 0;
    uint64_t delta = (system_time - last_debug_cycle) / 1000000ULL;
    if (delta >= 1000) {
        sched_debug_cycle(delta);
        last_debug_cycle = system_time;
    }
#endif

    struct list_head *iter, *b_iter;
    uintptr_t irq;
    spin_lock_irqsave(&g_sleep_lock, &irq);
    list_for_each_safe(iter, b_iter, &g_sleep_head) {
        struct io_notify *n = list_entry(iter, struct io_notify, link);
        struct thread *t = n->owner;

        if (!t) {
            list_del_init(iter);
            continue;
        }

        if (t->sleep_deadline <= system_time) {
            list_del_init(iter);
            thread_notify_io(n);
        }
    }
    spin_release_irqrestore(&g_sleep_lock, &irq);

    return IRQ_UNHANDLED;
}

void amd64_global_timer_init(void) {
    // Initialize global timer
    // Setup PIT
    outb(PIT_CMD, (3 << 4 /* Write lo/hi */) | (2 << 1 /* Rate generator */));
    outb(PIT_CH0, PIT_DIV & 0xFF);
    outb(PIT_CH0, PIT_DIV >> 8);

    irq_add_handler(2, timer_tick, (void *) TIMER_PIT);
    amd64_ioapic_unmask(2);
}

void amd64_timer_init(void) {
    if (get_cpu()->processor_id == 0) {
        amd64_global_timer_init();
    }

    // Initialize CPU-local timer
    kdebug("cpu%d: initializing timer\n", get_cpu()->processor_id);

    // No need to calibrate CPU-local timers - precision timer is global
    // LAPIC Timer is only used to trigger task switches
    LAPIC(LAPIC_REG_TMRDIV) = 0x3;
    LAPIC(LAPIC_REG_LVTT) = 32 | (1 << 17);
    LAPIC(LAPIC_REG_TMRINITCNT) = 150000;
    LAPIC(LAPIC_REG_TMRCURRCNT) = 0;

    get_cpu()->ticks = 0;
    asm volatile ("cli");
}
