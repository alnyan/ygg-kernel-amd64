#include "sys/amd64/hw/timer.h"
#include "sys/amd64/hw/ioapic.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/io.h"
#include "sys/spin.h"
#include "sys/debug.h"
#include "sys/amd64/cpu.h"
#include "sys/assert.h"

#define PIT_FREQ_BASE               1193182
#define PIT_CH0                     0x40
#define PIT_CMD                     0x43

// 10kHz calibration frequency = 100us period
#define PIT_CALIB_PERIOD            100
// Period of LAPIC timer in micros
#define TIMER_PERIOD                1000

#define PIT_CALIB_FREQ              (1000000 / (PIT_CALIB_PERIOD))
#define PIT_CALIB_TICKS             ((TIMER_PERIOD) / (PIT_CALIB_PERIOD))

static spin_t timer_lock = 0;
extern void amd64_irq2_counting(void);

void amd64_timer_init(void) {
    spin_lock(&timer_lock);
    kdebug("cpu%d: initializing timer\n", get_cpu()->processor_id);
    uint8_t lapic = LAPIC(LAPIC_REG_ID) >> 24;

    uint32_t low = amd64_ioapic_read(0x14);
    uint32_t high = amd64_ioapic_read(0x15);
    amd64_ioapic_map_gsi(2, lapic, 32);
    amd64_ioapic_unmask(2);
    amd64_idt_set(32, (uintptr_t) amd64_irq2_counting, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    get_cpu()->ticks = 0;

    uint16_t pit_div = PIT_FREQ_BASE / PIT_CALIB_FREQ;

    // Configure PIT
    outb(PIT_CMD, (0x3 << 4) | (1 << 2));
    outb(PIT_CH0, pit_div & 0xFF);
    outb(PIT_CH0, pit_div >> 8);

    // Divider = 3
    LAPIC(LAPIC_REG_TMRDIV) = 0x3;
    // InitCnt = -1
    LAPIC(LAPIC_REG_TMRINITCNT) = 0xFFFFFFFF;
    // Mask Local APIC timer
    LAPIC(LAPIC_REG_LVTT) = (1 << 16);

    asm volatile ("sti");
    while (get_cpu()->ticks < PIT_CALIB_TICKS) {
        asm volatile ("hlt");
    }
    asm volatile ("cli");

    // Ticks per TIMER_PERIOD
    uint32_t ticks = 0xFFFFFFFF - LAPIC(LAPIC_REG_TMRCURRCNT);

    LAPIC(LAPIC_REG_TMRDIV) = 0x3;
    LAPIC(LAPIC_REG_LVTT) = 32 | (1 << 17);
    LAPIC(LAPIC_REG_TMRINITCNT) = ticks;
    LAPIC(LAPIC_REG_TMRCURRCNT) = 0;

    get_cpu()->ticks = 0;

    amd64_ioapic_mask(2);
    amd64_idt_set(32, (uintptr_t) amd64_irq0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    spin_release(&timer_lock);
}
