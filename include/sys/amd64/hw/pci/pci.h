#pragma once
#include "sys/types.h"

#define PCI_PORT_CONFIG_ADDR        0xCF8
#define PCI_PORT_CONFIG_DATA        0xCFC

#define PCI_ID(vnd, dev)            (((uint32_t) (vnd)) | ((uint32_t) (dev) << 16))
#define PCI_MKADDR(bus, dev, func)  (((uint32_t) (bus) << 16) | ((uint32_t) (dev) << 8) | ((uint32_t) (func)))
#define PCI_FMTADDR                 "%02x:%02x:%02x"
#define PCI_VAADDR(addr)            ((addr) >> 16) & 0xFF, ((addr) >> 8) & 0xFF, (addr) & 0xFF
#define PCI_BUS(addr)               (((addr) >> 16) & 0xFF)
#define PCI_DEV(addr)               (((addr) >> 8) & 0xFF)
#define PCI_FUNC(addr)              ((addr) & 0xFF)

typedef uint32_t pci_addr_t;
typedef void (*pci_init_func_t) (pci_addr_t);

void pci_add_device_driver(uint32_t device_id, pci_init_func_t func);
void pci_add_class_driver(uint16_t class_id, pci_init_func_t func);
pci_init_func_t pci_find_driver(uint32_t device_id, uint16_t full_class);

void pci_config_write_dword(pci_addr_t addr, uint8_t off, uint32_t v);
uint32_t pci_config_read_dword(pci_addr_t addr, uint8_t off);

void pci_init(void);
void pci_add_root_bus(uint8_t n);

// pcidb.c
const char *pci_class_string(uint16_t full_class);
