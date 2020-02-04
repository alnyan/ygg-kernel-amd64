#pragma once
#include "sys/types.h"

struct ahci_registers;
struct pci_device;

struct ahci_fis_reg_h2d {
    uint8_t type;
    uint8_t cmd_port;
    uint8_t cmd;
    uint8_t feature_low;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t feature_high;

    uint16_t count;
    uint8_t icc;
    uint8_t control;

    uint32_t __res0;
};

struct ahci_controller {
    struct pci_device *pci_dev;
    uintptr_t abar_phys;
    struct ahci_registers *regs;
};
