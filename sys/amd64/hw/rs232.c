#include "arch/amd64/hw/rs232.h"
#include "arch/amd64/hw/io.h"

void rs232_init(uint16_t port) {
    // Do nothing yet
}

void rs232_send(uint16_t port, char v) {
    while (!(inb(port + 5) & 0x20)) {}
	outb(port, v);
}
