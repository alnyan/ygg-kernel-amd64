#pragma once
#include "sys/types.h"

#define PCI_PORT_CONFIG_ADDR        0xCF8
#define PCI_PORT_CONFIG_DATA        0xCFC

#define PCI_ID(vnd, dev)            (((uint32_t) (vnd)) | ((uint32_t) (dev) << 16))

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);

void pci_enumerate(void);
