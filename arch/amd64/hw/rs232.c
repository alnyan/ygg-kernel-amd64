#include "arch/amd64/hw/rs232.h"
#include "arch/amd64/hw/irq.h"
#include "arch/amd64/hw/io.h"
#include "sys/char/tty.h"
#include "sys/assert.h"
#include "sys/debug.h"

#define RS232_DTR       0
#define RS232_IER       1
#define RS232_ISR       2
#define RS232_LCR       3
#define RS232_MCR       4
#define RS232_LSR       5
#define RS232_MSR       6
#define RS232_SCR       7

#define RS232_LCR_8B    0x3
#define RS232_LCR_1SB   0
#define RS232_LCR_PNON  0

#define RS232_IER_RCE   1
#define RS232_IER_BRE   4

#define RS232_LSR_DR    1
#define RS232_LSR_OE    2
#define RS232_LSR_PE    4
#define RS232_LSR_FE    8
#define RS232_LSR_BI    16
#define RS232_LSR_THRE  32
#define RS232_LSR_TEMT  64
#define RS232_LSR_ERR   128

uint32_t rs232_avail = 0;

static int tty_serial_putc(void *ctx, int ch) {
    rs232_send((uintptr_t) ctx, ch);
    return 0;
}

static struct tty_serial tty_com1 = {
    .ctx = (void *) RS232_COM1,
    .tty = NULL,
    .putc = tty_serial_putc
};

void rs232_send(uint16_t port, char c) {
    while (!(inb(port + RS232_LSR) & RS232_LSR_THRE)) {
    }
    outb(port, c);
}

static uint32_t rs232_irq(void *ctx) {
    uint16_t port = (uint16_t) (uintptr_t) ctx;
    int has_data = 0;
    uint8_t s;

    while ((s = inb(port + RS232_LSR)) & RS232_LSR_DR) {
        has_data = 1;
        uint8_t c = inb(port);

        if (port == RS232_COM1 && tty_com1.tty) {
            tty_data_write(tty_com1.tty, c);
        }
    }

    return has_data ? IRQ_HANDLED : IRQ_UNHANDLED;
}

void rs232_add_tty(int n) {
    // No other implemented yet
    _assert(n == 0);

    tty_create_serial(&tty_com1);
}

void rs232_init(uint16_t port) {
    // Disable interrupts
    outb(port + RS232_IER, 0x00);

    // Set divisor entry mode
    // Base frequency is 115200, so just enter 1
    outb(port + RS232_LCR, 0x80);
    outb(port + RS232_DTR, 0x00);
    outb(port + RS232_IER, 0x01);

    // Set line control: 8 bits, 1 stop bit, no parity control
    outb(port + RS232_LCR, RS232_LCR_8B | RS232_LCR_1SB | RS232_LCR_PNON);

    outb(port + RS232_ISR, 0xC7);

    // Enable receive interrupts
    outb(port + RS232_IER, RS232_IER_RCE);

    switch (port) {
    case RS232_COM1:
        rs232_avail |= (1 << 0);
        break;
    }

    irq_add_leg_handler(IRQ_LEG_COM1_3, rs232_irq, (void *) RS232_COM1);
}
