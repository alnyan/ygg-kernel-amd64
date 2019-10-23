#include "sys/amd64/hw/pci/ahci.h"
#include "sys/amd64/hw/ide/ahci.h"
#include "sys/amd64/hw/ide/ata.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/mm/phys.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/mm.h"

// Only one controller is supported now
static struct pci_ahci ahci;

void pci_ahci_init(pci_addr_t addr) {
    kdebug("Initializing AHCI controller at " PCI_FMTADDR "\n", PCI_VAADDR(addr));

    uint32_t irq = pci_config_read_dword(addr, 0x3C);

    ahci.addr = addr;
    ahci.abar_phys = pci_config_read_dword(addr, 0x24);
    ahci.regs = (struct ahci_registers *) MM_VIRTUALIZE(ahci.abar_phys);

    kdebug("AHCI registers: %p\n", ahci.regs);

    ahci_init(ahci.regs);

    uint8_t int_pin = ((irq >> 8) & 0xFF);
    if (int_pin) {
        irq_add_pci_handler(addr, int_pin - 1, ahci_irq);
    }
}
