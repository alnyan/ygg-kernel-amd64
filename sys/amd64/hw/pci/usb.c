#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/hw/pci/usb_uhci.h"
#include "sys/debug.h"
#include "sys/attr.h"

static void pci_usb_init(pci_addr_t addr) {
    uint32_t class = pci_config_read_dword(addr, PCI_CONFIG_CLASS);
    uint8_t prog_if = (class >> 8) & 0xFF;

    switch (prog_if) {
#if defined(PCI_UHCI_ENABLE)
    case 0x00:
        pci_usb_uhci_init(addr);
        break;
#endif
    default:
        kwarn("Unsupported USB controller type: %02x\n", prog_if);
        break;
    }
}

static __init void pci_register_usb(void) {
    pci_add_class_driver(0x0C03, pci_usb_init);
}
