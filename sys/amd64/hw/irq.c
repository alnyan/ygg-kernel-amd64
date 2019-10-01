#include "sys/amd64/regs.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/amd64/hw/io.h"

void amd64_irq_handler(uint64_t n) {
    kdebug("IRQ #%ld\n", n);

    if (n == 1) {
        // Pop event from keyboard buffer
        inb(0x60);
    }

    // EOI
    if (n > 8) {
        panic("I forgot how to cascaded IRQs\n");
    }
    outb(0x20, 0x20);
}
