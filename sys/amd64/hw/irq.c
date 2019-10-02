#include "sys/amd64/regs.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/amd64/hw/io.h"
#include "sys/amd64/hw/ps2.h"

void amd64_irq_handler(uint64_t n) {
    if (n == 1) {
        amd64_irq1();
    }

    // EOI
    if (n > 8) {
        panic("I forgot how to cascaded IRQs\n");
    }
    outb(0x20, 0x20);
}
