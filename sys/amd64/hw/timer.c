#include "sys/amd64/hw/timer.h"
#include "sys/amd64/hw/ioapic.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/io.h"
#include "sys/spin.h"
#include "sys/time.h"
#include "sys/debug.h"
#include "sys/amd64/cpu.h"
#include "sys/assert.h"

#define TIMER_PIT                   1

#define PIT_FREQ_BASE               1193182
// Gives ~1kHz, ~1ms resolution
#define PIT_DIV                     1193
#define PIT_CH0                     0x40
#define PIT_CMD                     0x43

uint64_t int_timer_ticks = 0;

static uint32_t timer_tick(void *arg) {
    switch ((uint64_t) arg) {
    case TIMER_PIT:
        // Each tick is approx. 1ms, so add 1ms to system time
        system_time += 1000000;
        break;
    }
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
    LAPIC(LAPIC_REG_TMRINITCNT) = 1562500;
    LAPIC(LAPIC_REG_TMRCURRCNT) = 0;

    get_cpu()->ticks = 0;
}
