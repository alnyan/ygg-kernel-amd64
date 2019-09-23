#include "arch/amd64/hw/pic8259.h"
#include "arch/amd64/hw/io.h"

#define PIC_MCMD    0x20
#define PIC_MDAT    0x21
#define PIC_SCMD    0xA0
#define PIC_SDAT    0xA1
#define PIC_EOI     0x20

#define PIC_ICW1_ICW4       0x01
#define PIC_ICW1_SINGLE	    0x02
#define PIC_ICW1_INTERVAL4	0x04
#define PIC_ICW1_LEVEL	    0x08
#define PIC_ICW1_INIT	    0x10

#define PIC_ICW4_8086	    0x01
#define PIC_ICW4_AUTO	    0x02
#define PIC_ICW4_BUF_SLAVE	0x08
#define PIC_ICW4_BUF_MASTER	0x0C
#define PIC_ICW4_SFNM	    0x10

void pic8259_clear_mask(uint8_t line) {
    uint16_t port;
    uint8_t value;

    if (line < 8) {
        port = PIC_MDAT;
    } else {
        port = PIC_SDAT;
        line -= 8;
    }
    value = inb(port) & ~(1 << line);
    outb(port, value);
}

void pic8259_init(void) {
    uint8_t a1, a2;
	a1 = inb(PIC_MDAT);
	a2 = inb(PIC_SDAT);

	outb(PIC_MCMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
	outb(PIC_SCMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
	outb(PIC_MDAT, IRQ_BASE);
    io_wait();
	outb(PIC_SDAT, IRQ_BASE + 8);
    io_wait();
	outb(PIC_MDAT, 4);
    io_wait();
	outb(PIC_SDAT, 2);
    io_wait();

	outb(PIC_MDAT, PIC_ICW4_8086);
    io_wait();
    outb(PIC_SDAT, PIC_ICW4_8086);
    io_wait();

	outb(PIC_MDAT, a1);
	outb(PIC_SDAT, a2);
}

