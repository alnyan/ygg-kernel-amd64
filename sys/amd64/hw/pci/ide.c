#include "sys/amd64/hw/pci/ide.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"

void pci_ide_init(pci_addr_t addr) {
    kdebug("Initializing PCI IDE controller at " PCI_FMTADDR "\n", PCI_VAADDR(addr));

    uint32_t irq_config = pci_config_read_dword(addr, 0x3C);
    uint32_t class = pci_config_read_dword(addr, 0x08);

    if ((irq_config >> 8) & 0xFF) {
        panic("TODO: support IDE controllers with non-legacy IRQs\n");
    }

    irq_config &= ~0xFF;
    irq_config |= 0xFE;
    pci_config_write_dword(addr, 0x3C, irq_config);

    irq_config = pci_config_read_dword(addr, 0x3C);

    if ((irq_config & 0xFF) == 0xFE) {
        // Need IRQ assignment
        kdebug("Assigning ISA IRQ14 to IDE controller\n");

        // I guess using ISA 14 IRQ is a good idea
        irq_config &= ~0xFF;
        irq_config |= 14;

        pci_config_write_dword(addr, 0x3C, irq_config);
        irq_config = pci_config_read_dword(addr, 0x3C);

        assert((irq_config & 0xFF) == 14, "Failed to assign IRQ14 to IDE drive controller\n");
    } else {
        // The device either does not use IRQs or is a
        // paraller IDE with IRQs 14 and 15
        uint8_t prog_if = (class >> 8) & 0xFF;
        if (prog_if == 0x80 || prog_if == 0x8A) {
            // TODO: handle this case
            panic("This is a parallel IDE controller\n");
        } else {
            // TODO: handle this case
            panic("The device has no IRQs\n");
        }
    }
}
