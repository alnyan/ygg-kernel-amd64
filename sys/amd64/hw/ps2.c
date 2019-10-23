#include "sys/amd64/hw/ps2.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/io.h"

int ps2_irq_keyboard(void) {
    uint8_t st = inb(0x64);

    if (!(st & 1)) {
        return -1;
    }

    inb(0x60);

    return 0;
}

void ps2_init(void) {
    irq_add_leg_handler(IRQ_LEG_KEYBOARD, ps2_irq_keyboard);
}
