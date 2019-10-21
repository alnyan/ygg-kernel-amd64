#include "sys/amd64/hw/ide/ide.h"
#include "sys/amd64/hw/ide/ata.h"
#include "sys/amd64/hw/io.h"
#include "sys/debug.h"
#include "sys/panic.h"

static void ide_reg_write(struct ide_controller *ide, uint8_t channel, uint8_t reg, uint8_t data) {
    if (reg > 0x07 && reg < 0x0C) {
        ide_reg_write(ide, channel, ATA_REG_CONTROL, 0x80 | ide->channels[channel].no_int);
    }

    if (reg < 0x08) {
        outb(ide->channels[channel].base + reg - 0x00, data);
    } else if (reg < 0x0C) {
        outb(ide->channels[channel].base + reg - 0x06, data);
    } else if (reg < 0x0E) {
        outb(ide->channels[channel].ctrl + reg - 0x0A, data);
    } else if (reg < 0x16) {
        outb(ide->channels[channel].bmide + reg - 0x0E, data);
    }

    if (reg > 0x07 && reg < 0x0C) {
        ide_reg_write(ide, channel, ATA_REG_CONTROL, ide->channels[channel].no_int);
    }
}

uint8_t ide_reg_read(struct ide_controller *ide, uint8_t channel, uint8_t reg) {
   uint8_t result;

    if (reg > 0x07 && reg < 0x0C) {
        ide_reg_write(ide, channel, ATA_REG_CONTROL, 0x80 | ide->channels[channel].no_int);
    }

    if (reg < 0x08) {
        result = inb(ide->channels[channel].base + reg - 0x00);
    } else if (reg < 0x0C) {
        result = inb(ide->channels[channel].base + reg - 0x06);
    } else if (reg < 0x0E) {
        result = inb(ide->channels[channel].ctrl + reg - 0x0A);
    } else if (reg < 0x16) {
        result = inb(ide->channels[channel].bmide + reg - 0x0E);
    }

    if (reg > 0x07 && reg < 0x0C) {
        ide_reg_write(ide, channel, ATA_REG_CONTROL, ide->channels[channel].no_int);
    }
    return result;
}

void ide_reg_read_buf(struct ide_controller *ide, uint8_t channel, uint8_t reg, uint32_t *buf, size_t count) {
    if (reg > 0x07 && reg < 0x0C) {
        ide_reg_write(ide, channel, ATA_REG_CONTROL, 0x80 | ide->channels[channel].no_int);
    }

    while (count) {
        if (reg < 0x08) {
            *buf++ = inl(ide->channels[channel].base + reg - 0x00);
        } else if (reg < 0x0C) {
            *buf++ = inl(ide->channels[channel].base + reg - 0x06);
        } else if (reg < 0x0E) {
            *buf++ = inl(ide->channels[channel].ctrl + reg - 0x0A);
        } else if (reg < 0x16) {
            *buf++ = inl(ide->channels[channel].bmide + reg - 0x0E);
        }

        --count;
    }

    if (reg > 0x07 && reg < 0x0C) {
        ide_reg_write(ide, channel, ATA_REG_CONTROL, ide->channels[channel].no_int);
    }
}

// TODO: this is bad
#define ide_delay()     for (size_t i = 0; i < 1000000; ++i);

static int ide_wait_busy(struct ide_controller *ide, uint8_t ch) {
    uint8_t status;

    while (1) {
        status = ide_reg_read(ide, ch, ATA_REG_STATUS);
        if (status & ATA_SR_ERR) {
            return -1;
        }
        if (!(status & ATA_SR_BUSY)) {
            break;
        }
    }

    return 0;
}

void ide_init(struct ide_controller *ide) {
    // Initialize from PCI IDE information: BARs
    ide->channels[0].base = ide->bar0;
    ide->channels[0].ctrl = ide->bar1;
    ide->channels[0].bmide = ide->bar4;
    ide->channels[0].no_int = 0;

    ide->channels[1].base = ide->bar2;
    ide->channels[1].ctrl = ide->bar3;
    ide->channels[1].bmide = ide->bar4 + 8;
    ide->channels[1].no_int = 0;

    // Disable IRQs for both channels
    ide_reg_write(ide, 0, ATA_REG_CONTROL, ATA_CONTROL_NO_INT);
    ide_reg_write(ide, 1, ATA_REG_CONTROL, ATA_CONTROL_NO_INT);

    char ident_buf[512];

    for (size_t ch = 0; ch < 2; ++ch) {
        for (size_t sl = 0; sl < 2; ++sl) {
            struct ide_device *dev = &ide->devices[ch * 2 + sl];

            dev->attr = 0;

            ide_reg_write(ide, ch, ATA_REG_HDDEVSEL, 0xA0 | (sl << 4));
            ide_delay();

            ide_reg_write(ide, ch, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            ide_delay();

            if (ide_reg_read(ide, ch, ATA_REG_STATUS) == 0) {
                continue;
            }

            if (ide_wait_busy(ide, ch) != 0) {
                // May be an ATAPI device
                uint8_t b0 = ide_reg_read(ide, ch, ATA_REG_LBA1);
                uint8_t b1 = ide_reg_read(ide, ch, ATA_REG_LBA2);

                if ((b0 == 0x14 && b1 == 0xEB) || (b0 == 0x69 && b1 == 0x96)) {
                    dev->attr |= (1 << 3);
                } else {
                    // Skip invalid device
                    continue;
                }

                // Try again with correct ATAPI command
                ide_reg_write(ide, ch, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
                ide_delay();
            }

            // Read result from device's I/O buffer
            ide_reg_read_buf(ide, ch, ATA_REG_DATA, (uint32_t *) ident_buf, 128);

            // Set device present, channel and slave/master
            dev->attr |= (1 << 0) | (ch << 1) | (sl << 2);
            dev->caps = *(uint16_t *) (ident_buf + ATA_IDENT_CAPS);
            dev->signature = *(uint16_t *) (ident_buf + ATA_IDENT_DEVICE_TYPE);
            dev->cmd_sets = *(uint32_t *) (ident_buf + ATA_IDENT_CMD_SETS);

            if (dev->cmd_sets & (1 << 26)) {
                dev->size = *(uint32_t *) (ident_buf + ATA_IDENT_MAX_LBAEXT);
            } else {
                dev->size = *(uint32_t *) (ident_buf + ATA_IDENT_MAX_LBA);
            }

            for (size_t i = 0; i < 40; i += 2) {
                dev->model[i] = ident_buf[ATA_IDENT_MODEL + i + 1];
                dev->model[i + 1] = ident_buf[ATA_IDENT_MODEL + i];
            }
            // Trim padding whitespace
            for (size_t i = 39; i > 0; --i) {
                if (dev->model[i] != ' ') {
                    break;
                }
                dev->model[i] = 0;
            }
            dev->model[40] = 0;

            kdebug("Channel %s, %s, %s: %s, %S\n",
                ch ? "Secondary" : "Primary",
                sl ? "Slave" : "Master",
                dev->attr & (1 << 3) ? "ATAPI" : "ATA",
                dev->model,
                dev->size * 512);
        }
    }
}
