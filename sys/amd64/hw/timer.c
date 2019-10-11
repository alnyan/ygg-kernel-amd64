#include "sys/amd64/hw/timer.h"
#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/io.h"
#include "sys/spin.h"
#include "sys/debug.h"

#define PIT_FREQ_BASE               1193182
#define PIT_CH0                     0x40
#define PIT_CMD                     0x43

static spin_t amd64_timer_spin = 0;
// TODO: proper GS-based smp_per_cpu(x) macro for per-CPU variable access
extern uint64_t amd64_irq0_counting;

void amd64_timer_init(void) {
    // XXX: Frequency is ignore for now:
    //      Once I set the I/O APIC up,
    //      PIT will be used for proper
    //      timer calibration
    LAPIC(LAPIC_REG_TMRDIV) = 0x3;
    LAPIC(LAPIC_REG_TMRINITCNT) = 10000000;
    LAPIC(LAPIC_REG_LVTT) = 32 | (1 << 17);
}
