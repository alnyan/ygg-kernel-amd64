#include "sys/amd64/hw/timer.h"
#include "sys/amd64/hw/io.h"
#include "sys/amd64/acpi/hpet.h"
#include "sys/debug.h"

// IO ports
#define PIT_CH0         0x40
#define PIT_CMD         0x43

// Select channel
#define PIT_BIT_CH0     (0x0 << 6)
#define PIT_BIT_CH1     (0x1 << 6)
#define PIT_BIT_CH2     (0x2 << 6)

// Access flags
#define PIT_ACC_LATCH   (0x0 << 4)
#define PIT_ACC_LOW     (0x1 << 4)
#define PIT_ACC_HIGH    (0x2 << 4)
#define PIT_ACC_16      (0x3 << 4)

// Operation mode
#define PIT_MODE_CDOWN  (0x0 << 1)
#define PIT_MODE_ONESH  (0x1 << 1)
#define PIT_MODE_RATEG  (0x2 << 1)
#define PIT_MODE_SQWAV  (0x3 << 1)
#define PIT_MODE_SWSTR  (0x4 << 1)
#define PIT_MODE_HWSTR  (0x5 << 1)

// BCD/Binary mode
#define PIT_BCD_NO      (0 << 0)
#define PIT_BCD_YES     (0 << 1)

#define PIT_FREQ_BASE   1193182

uint64_t amd64_timer_ticks = 0;
void (*amd64_timer_tick) (void) = NULL;

static void pit8253_init(void) {
    uint32_t div = PIT_FREQ_BASE / 1000;

    outb(PIT_CMD, PIT_CH0 | PIT_BCD_NO | PIT_ACC_16 | PIT_MODE_RATEG);
    outb(PIT_CH0, div & 0xFF);
    outb(PIT_CH0, (div >> 8) & 0xFF);
}

static void pit8253_tick(void) {
    ++amd64_timer_ticks;
}

void amd64_timer_configure(void) {
    if (acpi_hpet_init() != 0) {
        kdebug("HPET is not available, falling back to PIT\n");
        amd64_timer_tick = pit8253_tick;
        pit8253_init();
    }
}
