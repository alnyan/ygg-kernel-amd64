#include "sys/amd64/hw/timer.h"
#include "sys/debug.h"

#define IA32_LAPIC_REG_LVTT         0x320

#define IA32_LAPIC_REG_TMRINITCNT   0x380
#define IA32_LAPIC_REG_TMRCURRCNT   0x390
#define IA32_LAPIC_REG_TMRDIV       0x3E0

void amd64_timer_init(uint32_t fq) {
    // XXX: Frequency is ignore for now:
    //      Once I set the I/O APIC up,
    //      PIT will be used for proper
    //      timer calibration

    *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_TMRDIV) = 0x3;
    *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_TMRINITCNT) = 10000000;
    *(uint32_t *) (0xFFFFFF00FEE00000 + IA32_LAPIC_REG_LVTT) = 32 | (1 << 17);
}
