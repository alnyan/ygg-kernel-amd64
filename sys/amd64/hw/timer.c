#include "sys/amd64/hw/timer.h"
#include "sys/amd64/hw/io.h"
#include "sys/spin.h"
#include "sys/debug.h"

#define IA32_LAPIC_REG_LVTT         0x320

#define IA32_LAPIC_REG_TMRINITCNT   0x380
#define IA32_LAPIC_REG_TMRCURRCNT   0x390
#define IA32_LAPIC_REG_TMRDIV       0x3E0

#define PIT_FREQ_BASE               1193182
#define PIT_CH0                     0x40
#define PIT_CMD                     0x43

static spin_t amd64_timer_spin = 0;
// TODO: proper GS-based smp_per_cpu(x) macro for per-CPU variable access
uint64_t amd64_timer_counter[AMD64_MAX_SMP] = {0};
extern uint64_t amd64_irq0_counting;

void amd64_timer_init(void) {
    // XXX: Frequency is ignore for now:
    //      Once I set the I/O APIC up,
    //      PIT will be used for proper
    //      timer calibration
    spin_lock(&amd64_timer_spin);

    // Here LT = LAPIC Timer

    // LT.Divider = 16
    *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_TMRDIV) = 0x3;

    // Mask LAPIC timer
    *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_LVTT) = (1 << 16);
    *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_TMRINITCNT) = 0xFFFFFFFF;

    // XXX: Broken with SMP until I add per-cpu variables
    amd64_timer_counter[0] = 0;

    // Freq: 10kHz, IRQ every 10us
    uint16_t pit_div = (uint16_t) (PIT_FREQ_BASE / 100000);
    outb(PIT_CMD, (0x2 << 1) | (0x3 << 4));
    outb(PIT_CH0, pit_div & 0xFF);
    outb(PIT_CH0, pit_div >> 8);

    amd64_irq0_counting = 1;
    asm volatile ("sti; hlt; cli");
    amd64_irq0_counting = 0;

    uint32_t reload = 0xFFFFFFFF - *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_TMRCURRCNT);

    *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_TMRINITCNT) = reload;
    *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_LVTT) = 32 | (1 << 17);

    amd64_timer_counter[0] = 0;

    spin_release(&amd64_timer_spin);
}
