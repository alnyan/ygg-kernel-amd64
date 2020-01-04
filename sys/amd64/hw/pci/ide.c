#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/hw/ide/ide.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/attr.h"

// XXX: only one IDE controller per host
static struct ide_controller pci_ide;

static void pci_ide_init(pci_addr_t addr) {
    kdebug("Initializing PCI IDE controller at " PCI_FMTADDR "\n", PCI_VAADDR(addr));

    uint32_t irq_config = pci_config_read_dword(addr, PCI_CONFIG_IRQ);
    uint32_t class = pci_config_read_dword(addr, PCI_CONFIG_CLASS);
    uint8_t prog_if = (class >> 8) & 0xFF;
    uint32_t bar;
    uint32_t cmd = pci_config_read_dword(addr, PCI_CONFIG_CMD);

    // Read BARs
    bar = pci_config_read_dword(addr, PCI_CONFIG_BAR(0));
    assert(!bar || (bar & 1), "IDE BAR not in I/O space\n");
    if (bar & ~3) {
        pci_ide.bar0 = bar & ~3;
    } else {
        pci_ide.bar0 = IDE_DEFAULT_BAR0;
    }
    bar = pci_config_read_dword(addr, PCI_CONFIG_BAR(1));
    assert(!bar || (bar & 1), "IDE BAR not in I/O space\n");
    if (bar & ~3) {
        pci_ide.bar1 = bar & ~3;
    } else {
        pci_ide.bar1 = IDE_DEFAULT_BAR1;
    }
    bar = pci_config_read_dword(addr, PCI_CONFIG_BAR(2));
    assert(!bar || (bar & 1), "IDE BAR not in I/O space\n");
    if (bar & ~3) {
        pci_ide.bar2 = bar & ~3;
    } else {
        pci_ide.bar2 = IDE_DEFAULT_BAR2;
    }
    bar = pci_config_read_dword(addr, PCI_CONFIG_BAR(3));
    assert(!bar || (bar & 1), "IDE BAR not in I/O space\n");
    if (bar & ~3) {
        pci_ide.bar3 = bar & ~3;
    } else {
        pci_ide.bar3 = IDE_DEFAULT_BAR3;
    }
    bar = pci_config_read_dword(addr, PCI_CONFIG_BAR(4));
    assert(!bar || (bar & 1), "IDE BAR not in I/O space\n");
    pci_ide.bar4 = bar & ~3;
    if (!pci_ide.bar4) {
        kwarn("No IDE busmastering BAR4 in PCI configuration space\n");
    }

    if ((irq_config >> 8) & 0xFF) {
        panic("TODO: support IDE controllers with non-legacy IRQs\n");
    }

    // Enable PCI busmastering for IDE controller
    cmd |= (1 << 2);
    pci_config_write_dword(addr, PCI_CONFIG_CMD, cmd);

    irq_config &= ~0xFF;
    irq_config |= 0xFE;
    pci_config_write_dword(addr, PCI_CONFIG_IRQ, irq_config);

    irq_config = pci_config_read_dword(addr, PCI_CONFIG_IRQ);

    if ((irq_config & 0xFF) == 0xFE) {
        // Need IRQ assignment
        kdebug("Assigning ISA IRQ14 to IDE controller\n");

        // I guess using ISA 14 IRQ is a good idea
        irq_config &= ~0xFF;
        irq_config |= 14;

        pci_config_write_dword(addr, PCI_CONFIG_IRQ, irq_config);
        irq_config = pci_config_read_dword(addr, PCI_CONFIG_IRQ);

        assert((irq_config & 0xFF) == 14, "Failed to assign IRQ14 to IDE drive controller\n");

        pci_ide.irq0 = 14;
    } else {
        // The device either does not use IRQs or is a
        // paraller IDE with IRQs 14 and 15
        if (prog_if == 0x80 || prog_if == 0x8A) {
            // TODO: handle this case
            //panic("This is a parallel IDE controller\n");
            return;
        } else {
            // TODO: handle this case
            panic("The device has no IRQs\n");
        }
    }

    ide_init(&pci_ide);
}

static __init void pci_ide_register(void) {
    pci_add_class_driver(0x0101, pci_ide_init);
}
