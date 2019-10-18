#pragma once
#include "sys/types.h"

#define PCI_PORT_CONFIG_ADDR        0xCF8
#define PCI_PORT_CONFIG_DATA        0xCFC

#define PCI_ID(vnd, dev)            (((uint32_t) (vnd)) | ((uint32_t) (dev) << 16))

void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t v);
uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);

void pci_init(void);
void pci_add_root_bus(uint8_t n);

// pcidb.c
const char *pci_class_string(uint16_t full_class);
