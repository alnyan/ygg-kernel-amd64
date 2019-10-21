#pragma once
#include "pci.h"
#include "sys/amd64/hw/ide/ahci.h"

struct pci_ahci {
    pci_addr_t addr;
    uint32_t abar_phys;
    struct ahci_registers *volatile regs;
};

void pci_ahci_init(pci_addr_t addr);
