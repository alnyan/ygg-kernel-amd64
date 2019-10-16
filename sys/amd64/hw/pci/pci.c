#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/hw/io.h"
#include "sys/debug.h"

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    uint32_t w0;
    w0 = ((uint32_t) bus << 16) |
         ((uint32_t) slot << 11) |
         ((uint32_t) func << 8) |
         ((uint32_t) off & ~0x3) |
         (1 << 31);

    outl(PCI_PORT_CONFIG_ADDR, w0);
    w0 = inl(PCI_PORT_CONFIG_DATA);

    return w0;
}

static void pci_enumerate_func(uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t id = pci_config_read_dword(bus, dev, func, 0);
    uint32_t kind = pci_config_read_dword(bus, dev, func, 8);
    uint32_t irq = pci_config_read_dword(bus, dev, func, 0x3C);

    // First find devices matching known IDs
    switch (id) {
    case PCI_ID(0x10EC, 0x8139):
        // Think I'll implement this later
        kdebug("rtl8139\n");
        kdebug("PCI INT%c#, PIC IRQ%d\n", ('A' + ((irq >> 8) & 0xFF) - 1), irq & 0xFF);
        break;
    }

    // Check for generic-driver devices
    switch (kind >> 16) {
    case 0x0101:
        kdebug("IDE drive controller\n");
        break;
    }
}

static void pci_enumerate_dev(uint8_t bus, uint8_t dev) {
    // Check function 0
    uint32_t w0 = pci_config_read_dword(bus, dev, 0, 0);
    if ((w0 & 0xFFFF) == 0xFFFF) {
        // No functions here
        return;
    }

    for (uint8_t func = 0; func < 8; ++func) {
        if (!func || (((w0 = pci_config_read_dword(bus, dev, func, 0)) & 0xFFFF) != 0xFFFF)) {
            pci_enumerate_func(bus, dev, func);
        }
    }
}

void pci_enumerate(void) {
    for (uint8_t bus = 0; bus < 255; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            pci_enumerate_dev(bus, dev);
        }
    }
}
