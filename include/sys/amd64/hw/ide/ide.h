#pragma once
#include "sys/types.h"

#define IDE_DEFAULT_BAR0        0x1F0
#define IDE_DEFAULT_BAR1        0x3F6
#define IDE_DEFAULT_BAR2        0x170
#define IDE_DEFAULT_BAR3        0x376

struct ide_prdt_entry {
    uint32_t addr;
    uint16_t size;
    uint16_t attr;
} __attribute__((packed));

struct ide_controller {
    uint16_t bar0;      // Primary channel
    uint16_t bar1;      // Primary control port
    uint16_t bar2;      // Secondary channel
    uint16_t bar3;      // Secondary control port
    uint16_t bar4;      // Bus master control
    uint8_t irq0, irq1;

    uintptr_t dma_page;

    struct ide_channel {
        uint16_t base;
        uint16_t ctrl;
        uint16_t bmide;
        uint8_t no_int;
    } channels[2];

    struct ide_device {
        // Bits:
        // 0        1 if device is present here
        // 1        0 if on primary channel
        // 2        1 if slave device
        // 3        1 if device is ATAPI (ATA otherwise)
        // ...
        struct ide_controller *ide;
        uint8_t attr;
        uint16_t signature;
        uint16_t caps;
        uint32_t cmd_sets;
        size_t size;
        char model[41];
    } devices[4];
};

int ide_ata_read_pio(struct ide_device *dev, void *buf, size_t nsect, uint64_t lba);
int ide_ata_read_dma(struct ide_device *dev, void *buf, size_t nsect, uint64_t lba);

void ide_init(struct ide_controller *ide);
