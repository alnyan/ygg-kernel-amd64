#include "sys/amd64/hw/pci/pci.h"
#include "sys/debug.h"

void pci_ahci_init(pci_addr_t addr) {
    kdebug("Initializing AHCI controller at " PCI_FMTADDR "\n", PCI_VAADDR(addr));
}
