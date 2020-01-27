#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/hw/ioapic.h"
#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/io.h"
#include "sys/panic.h"
#include "sys/assert.h"
#include "sys/debug.h"

void pci_add_root_bus(uint8_t n) {
    kwarn("%s: %02x\n", __func__, n);
}

void pci_init(void) {
}
