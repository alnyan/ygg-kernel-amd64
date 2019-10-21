#include "sys/amd64/hw/ide/ide.h"
#include "sys/amd64/hw/ide/ata.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/io.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/mm.h"

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

void ide_reg_read_dbuf(struct ide_controller *ide, uint8_t channel, uint8_t reg, uint32_t *buf, size_t count) {
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

void ide_reg_read_wbuf(struct ide_controller *ide, uint8_t channel, uint8_t reg, uint16_t *buf, size_t count) {
    if (reg > 0x07 && reg < 0x0C) {
        ide_reg_write(ide, channel, ATA_REG_CONTROL, 0x80 | ide->channels[channel].no_int);
    }

    while (count) {
        if (reg < 0x08) {
            *buf++ = inw(ide->channels[channel].base + reg - 0x00);
        } else if (reg < 0x0C) {
            *buf++ = inw(ide->channels[channel].base + reg - 0x06);
        } else if (reg < 0x0E) {
            *buf++ = inw(ide->channels[channel].ctrl + reg - 0x0A);
        } else if (reg < 0x16) {
            *buf++ = inw(ide->channels[channel].bmide + reg - 0x0E);
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

static int ide_wait_poll(struct ide_controller *ide, uint8_t ch) {
    // 400ns delay
    for(int i = 0; i < 4; i++) {
        ide_reg_read(ide, ch, ATA_REG_ALTSTATUS);
    }

    return ide_wait_busy(ide, ch);
}

int ide_ata_set_addr(struct ide_device *dev, uint64_t lba, size_t nsect) {
    struct ide_controller *ide = dev->ide;
    uint8_t ch = (dev->attr >> 1) & 1;
    uint8_t sl = (dev->attr >> 2) & 1;

    uint8_t lba_buf[6] = {0};
    int lba_mode;

    uint16_t cyl;
    uint8_t head, sect = 0;

    if (lba >= 0x10000000) {
        lba_mode = 2;
        // LBA48
        lba_buf[0] = lba & 0xFF;
        lba_buf[1] = (lba >> 8) & 0xFF;
        lba_buf[2] = (lba >> 16) & 0xFF;
        lba_buf[3] = (lba >> 24) & 0xFF;
        lba_buf[4] = (lba >> 32) & 0xFF;
        lba_buf[5] = (lba >> 40) & 0xFF;
    } else if (dev->caps & 0x200) {
        lba_mode = 1;
        // LBA24
        lba_buf[0] = lba & 0xFF;
        lba_buf[1] = (lba >> 8) & 0xFF;
        lba_buf[1] = (lba >> 16) & 0xFF;
    } else {
        lba_mode = 0;

        sect = (lba % 63) + 1;
        cyl = (lba + 1 - sect) / (16 * 63);
        head = (lba + 1 - sect) % (16 * 63) / (63);

        // CHS
        lba_buf[0] = sect;
        lba_buf[1] = cyl & 0xFF;
        lba_buf[2] = cyl >> 8;
    }

    while (ide_reg_read(ide, ch, ATA_REG_STATUS) & ATA_SR_BUSY);

    if (lba_mode) {
        ide_reg_write(ide, ch, ATA_REG_HDDEVSEL, 0xE0 | (sl << 4));
    } else {
        ide_reg_write(ide, ch, ATA_REG_HDDEVSEL, 0xA0 | (sl << 4) | head);
    }

    if (lba_mode == 2) {
        ide_reg_write(ide, ch, ATA_REG_SECCOUNT1, 0);
        ide_reg_write(ide, ch, ATA_REG_LBA3, lba_buf[3]);
        ide_reg_write(ide, ch, ATA_REG_LBA4, lba_buf[4]);
        ide_reg_write(ide, ch, ATA_REG_LBA5, lba_buf[5]);
    }

    ide_reg_write(ide, ch, ATA_REG_SECCOUNT0, nsect);
    ide_reg_write(ide, ch, ATA_REG_LBA0, lba_buf[0]);
    ide_reg_write(ide, ch, ATA_REG_LBA1, lba_buf[1]);
    ide_reg_write(ide, ch, ATA_REG_LBA2, lba_buf[2]);

    return lba_mode;
}

int ide_ata_read_dma(struct ide_device *dev, void *buf, size_t nsect, uint64_t lba) {
    struct ide_controller *ide = dev->ide;
    uint8_t ch = (dev->attr >> 1) & 1;
    uint8_t sl = (dev->attr >> 2) & 1;

    // Disable IRQs
    ide_reg_write(ide, ch, ATA_REG_CONTROL, ATA_CONTROL_NO_INT);

    // TODO: wait until previous DMA operation finishes
    struct ide_prdt_entry *ent = (struct ide_prdt_entry *) MM_VIRTUALIZE(ide->dma_page + ch * 2048);

    size_t bytes_count = nsect * 512;
    size_t bytes_left = bytes_count;
    size_t prd_count = (bytes_count + 65535) / 65536;
    // TODO: do this properly: check alignments etc
    uintptr_t buf_phys = MM_PHYS(buf);

    // Fill out PRDT
    for (size_t i = 0; i < prd_count; ++i) {
        size_t prd_size = MIN(65536, bytes_left);
        _assert((buf_phys + i * 65536) < 0xFFFFFFFF);
        ent[i].addr = (buf_phys + i * 65536) & 0xFFFFFFFF;
        ent[i].size = prd_size == 65536 ? 0 : prd_size;
        ent[i].attr = 0;
        if (i == prd_count - 1) {
            ent[i].attr = 1 << 15;
        }
        bytes_left -= prd_size;
    }

    // Start DMA transfer
    outb(ide->channels[ch].bmide + 0x00, 1);

    int lba_mode = ide_ata_set_addr(dev, lba, nsect);
    uint8_t cmd;
    if (lba_mode == 2) {
        cmd = ATA_CMD_READ_DMA_EX;
    } else {
        cmd = ATA_CMD_READ_DMA;
    }

    ide_reg_write(ide, ch, ATA_REG_COMMAND, cmd);

    // Wait for the drive to receive the command before sending another one
    if (ide_wait_poll(ide, ch) < 0) {
        panic("Read failed\n");
    }

    // TODO: IRQs?
    while (1) {
        uint8_t status = inb(ide->channels[ch].bmide + 0x02);

        if (status & (1 << 1)) {
            panic("DMA error\n");
        }

        if (!(status & (1 << 0))) {
            break;
        }
    }

    return 0;
}
int ide_ata_read_pio(struct ide_device *dev, void *buf, size_t nsect, uint64_t lba) {
    struct ide_controller *ide = dev->ide;
    uint8_t ch = (dev->attr >> 1) & 1;
    uint8_t sl = (dev->attr >> 2) & 1;

    // Disable IRQs
    ide_reg_write(ide, ch, ATA_REG_CONTROL, ATA_CONTROL_NO_INT);

    int lba_mode = ide_ata_set_addr(dev, lba, nsect);

    uint8_t cmd;
    if (lba_mode == 2) {
        cmd = ATA_CMD_READ_PIO_EX;
    } else {
        cmd = ATA_CMD_READ_PIO;
    }

    ide_reg_write(ide, ch, ATA_REG_COMMAND, cmd);

    uint16_t *word = (uint16_t *) buf;
    for (size_t i = 0; i < nsect; ++i) {
        if (ide_wait_poll(ide, ch) != 0) {
            return -1;
        }

        // Read a sector
        ide_reg_read_wbuf(ide, ch, ATA_REG_DATA, word + 256 * i, 256);
    }

    return 0;
}

static char test_buf[65536 * 2] __attribute__((aligned(65536)));

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

    // Setup busmastering DMA
    ide->dma_page = amd64_phys_alloc_page();
    _assert(ide->dma_page != MM_NADDR);
    outb(ide->channels[0].bmide + 0x00, 0);
    outb(ide->channels[0].bmide + 0x04, ide->dma_page & 0xFF);
    outb(ide->channels[0].bmide + 0x05, (ide->dma_page >> 8) & 0xFF);
    outb(ide->channels[0].bmide + 0x06, (ide->dma_page >> 16) & 0xFF);
    outb(ide->channels[0].bmide + 0x07, (ide->dma_page >> 24) & 0xFF);
    outb(ide->channels[1].bmide + 0x00, 0);
    outb(ide->channels[1].bmide + 0x04, (ide->dma_page + 2048) & 0xFF);
    outb(ide->channels[1].bmide + 0x05, ((ide->dma_page + 2048) >> 8) & 0xFF);
    outb(ide->channels[1].bmide + 0x06, ((ide->dma_page + 2048) >> 16) & 0xFF);
    outb(ide->channels[1].bmide + 0x07, ((ide->dma_page + 2048) >> 24) & 0xFF);

    char ident_buf[1024];

    for (size_t ch = 0; ch < 2; ++ch) {
        for (size_t sl = 0; sl < 2; ++sl) {
            struct ide_device *dev = &ide->devices[ch * 2 + sl];
            dev->ide = ide;

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
            ide_reg_read_dbuf(ide, ch, ATA_REG_DATA, (uint32_t *) ident_buf, 128);

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
