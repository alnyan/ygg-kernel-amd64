#include "sys/amd64/hw/ide/ahci.h"
#include "sys/amd64/hw/ide/ata.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/mm/phys.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/dev.h"
#include "sys/blk.h"
#include "sys/mm.h"

// Memory required for a port:
// 1024     - command list  (32 * sizeof(ahci_command_header))
// 256      - received FIS
// 8192     - command table (32 * sizeof(ahci_command_table_entry))
// So we need at least 3 pages for all the stuff (buffers not included)

static int ahci_alloc_cmd(struct ahci_port_registers *port) {
    uint32_t slots = (port->p_sact | port->p_ci);
    for (int i = 0; i < 32; ++i) {
        if (!(slots & (1 << i))) {
            return i;
        }
    }
    return -1;
}

static void ahci_irq_port(struct ahci_port_registers *port) {
    if (port->p_is & (1 << 0)) {
        // TODO: integrate this with reading code
    }

    port->p_is = 0;
}

static ssize_t ahci_blk_read(struct blkdev *blk, void *buf, size_t off, size_t count) {
    _assert(blk && blk->dev_data);
    _assert(!(off & 511));
    _assert(!(count & 511));

    size_t nsect = count / 512;
    size_t lba = off / 512;

    // TODO: error?
    ahci_sata_read((struct ahci_port_registers *) blk->dev_data, buf, nsect, lba);

    return count;
}

static ssize_t ahci_blk_write(struct blkdev *blk, const void *buf, size_t off, size_t count) {
    _assert(blk && blk->dev_data);
    _assert(!(off & 511));
    _assert(!(count & 511));

    size_t nsect = count / 512;
    size_t lba = off / 512;

    ahci_sata_write((struct ahci_port_registers *) blk->dev_data, buf, nsect, lba);

    return count;
}

static int ahci_add_port_dev(struct ahci_port_registers *port) {
    void *buf = kmalloc(sizeof(struct blkdev) + sizeof(struct dev_entry));
    struct dev_entry *ent = (struct dev_entry *) buf;
    struct blkdev *blk = (struct blkdev *) ((uintptr_t) buf + sizeof(struct dev_entry));
    _assert(ent);

    blk->dev_data = port;
    blk->read = ahci_blk_read;
    blk->write = ahci_blk_write;

    ent->dev = blk;
    ent->dev_class = DEV_CLASS_BLOCK;
    ent->dev_subclass = DEV_BLOCK_SDx;
    if (dev_alloc_name(ent->dev_class, ent->dev_subclass, ent->dev_name) != 0) {
        return -1;
    }

    dev_entry_add(ent);

    return 0;
}

uint32_t ahci_irq(void *ctx) {
    struct ahci_registers *controller = ctx;

    if (controller->is) {
        for (size_t port = 0; port < 32; ++port) {
            ahci_irq_port(&controller->ports[port]);
            controller->is &= ~(1 << port);
        }

        return IRQ_HANDLED;
    }

    return IRQ_UNHANDLED;
}

static void ahci_sata_access(struct ahci_port_registers *port, void *buf, uint32_t nsect, uint64_t lba, uint8_t dir) {
    kdebug("%s access to AHCI drive\n", dir ? "Write" : "Read");
    port->p_is = -1;

    int cmd = ahci_alloc_cmd(port);
    _assert(cmd != -1);

    // Command list
    struct ahci_command_header *cmd_list = AHCI_PORT_CMD_LIST(port);
    struct ahci_command_header *cmd_header = &cmd_list[cmd];
    // Command table entry for the slot
    struct ahci_command_table_entry *cmd_table = AHCI_CMD_TABLE_ENTRY(cmd_list, cmd);

    size_t byte_count = nsect * 512;
    // Maximum PRDT entry size is 8K, so maximum read from disk using this call is 64K
    size_t prd_count = (byte_count + 8191) / 8192;

    if (prd_count > 8) {
        panic("Too many PRDT entries\n");
    }

    size_t bytes_left = byte_count;

    cmd_header->attr = sizeof(struct ahci_fis_reg_h2d) / sizeof(uint32_t);
    cmd_header->prdtl = prd_count;
    cmd_header->prdbc = 0;

    uintptr_t buf_phys = ((uintptr_t) buf) - 0xFFFFFF0000000000;

    memset(cmd_table, 0, sizeof(struct ahci_command_table_entry) + sizeof(struct ahci_prdt_entry) * prd_count);

    for (size_t i = 0; i < prd_count; ++i) {
        size_t prd_size = MIN(8192, bytes_left);
        _assert(prd_size);
        uintptr_t prd_buf = buf_phys + 8192 * i;

        cmd_table->prdt[i].dba = prd_buf & 0xFFFFFFFF;
        cmd_table->prdt[i].dbau = prd_buf >> 32;
        cmd_table->prdt[i].dbc = ((prd_size - 1) << 1) | 1;

        kdebug("PRD%d: %S at %p\n", i, prd_size, prd_buf);

        if (i == prd_count - 1) {
            // XXX: Enable interrupt on last entry?
            cmd_table->prdt[i].dbc |= 1 << 31;
        }

        bytes_left -= prd_size;
    }

    _assert(!bytes_left);

    // Prepare command
    struct ahci_fis_reg_h2d *fis_reg_h2d = &cmd_table->fis_reg_h2d;
    memset(fis_reg_h2d, 0, sizeof(struct ahci_fis_reg_h2d));
    fis_reg_h2d->type = FIS_REG_H2D;
    fis_reg_h2d->cmd = dir ? ATA_CMD_WRITE_DMA_EX : ATA_CMD_READ_DMA_EX;
    fis_reg_h2d->cmd_port = 1 << 7;
    fis_reg_h2d->lba0 = lba & 0xFF;
    fis_reg_h2d->lba1 = (lba >> 8) & 0xFF;
    fis_reg_h2d->lba2 = (lba >> 16) & 0xFF;
    fis_reg_h2d->dev = (1 << 6);         // LBA mode
    fis_reg_h2d->lba3 = (lba >> 24) & 0xFF;
    fis_reg_h2d->lba4 = (lba >> 32) & 0xFF;
    fis_reg_h2d->lba5 = (lba >> 40) & 0xFF;
    fis_reg_h2d->countl = nsect & 0xFF;
    fis_reg_h2d->counth = (nsect >> 8) & 0xFF;

    uint32_t spin = 0;
    while ((port->p_tfd & (ATA_SR_BUSY | ATA_SR_DRQ)) && spin < 1000000) {
        ++spin;
    }
    if (spin == 1000000) {
        panic("SATA port hang\n");
    }

    port->p_ci |= 1 << cmd;

    while (1) {
        // TODO: interrupts?

        if (!(port->p_ci & (1 << cmd))) {
            break;
        }

        if (port->p_is & AHCI_PORT_IS_TFES) {
            panic("Port task file error\n");
        }
    }

    if (port->p_is & AHCI_PORT_IS_TFES) {
        panic("Port task file error\n");
    }
}

void ahci_sata_read(struct ahci_port_registers *port, void *buf, uint32_t nsect, uint64_t lba) {
    ahci_sata_access(port, buf, nsect, lba, 0);
}

void ahci_sata_write(struct ahci_port_registers *port, const void *buf, uint32_t nsect, uint64_t lba) {
    ahci_sata_access(port, (void *) buf, nsect, lba, 1);
}

static void ahci_sata_port_identify(struct ahci_port_registers *port) {
    port->p_is = -1;

    int cmd = ahci_alloc_cmd(port);
    _assert(cmd != -1);

    // Command list
    struct ahci_command_header *cmd_list = AHCI_PORT_CMD_LIST(port);
    struct ahci_command_header *cmd_header = &cmd_list[cmd];
    // Command table entry for the slot
    struct ahci_command_table_entry *cmd_table = AHCI_CMD_TABLE_ENTRY(cmd_list, cmd);

    // One PRDT entry
    char ident_buf[1024] = {0};
    uintptr_t ident_buf_phys = ((uintptr_t) ident_buf) - 0xFFFFFF0000000000;

    cmd_header->attr = (sizeof(struct ahci_fis_reg_h2d) / 4);
    cmd_header->prdtl = 1;

    memset(cmd_table, 0, sizeof(struct ahci_command_table_entry) + sizeof(struct ahci_prdt_entry));
    cmd_table->prdt[0].dba = ident_buf_phys & 0xFFFFFFFF;
    cmd_table->prdt[0].dbau = ident_buf_phys >> 32;
    cmd_table->prdt[0].dbc = 1 | ((sizeof(ident_buf) - 1) << 1);

    struct ahci_fis_reg_h2d *fis_reg_h2d = &cmd_table->fis_reg_h2d;
    memset(fis_reg_h2d, 0, sizeof(struct ahci_fis_reg_h2d));
    fis_reg_h2d->type = FIS_REG_H2D;
    fis_reg_h2d->cmd = ATA_CMD_IDENTIFY;
    fis_reg_h2d->cmd_port = 1 << 7;

    uint32_t spin = 0;
    while ((port->p_tfd & (ATA_SR_BUSY | ATA_SR_DRQ)) && spin < 1000000) {
        ++spin;
    }
    if (spin == 1000000) {
        panic("SATA port hang\n");
    }

    port->p_ci |= 1 << cmd;

    while (1) {
        if (!(port->p_ci & (1 << cmd))) {
            break;
        }

        if (port->p_is & AHCI_PORT_IS_TFES) {
            panic("Port task file error\n");
        }
    }

    kdebug("Drive identification:\n");

    char model_str[41] = {0};
    uint32_t cmd_sets = *(uint32_t *) (ident_buf + ATA_IDENT_CMD_SETS);
    uint32_t disk_size = 0;
    for (size_t i = 0; i < 40; i += 2) {
        model_str[i] = ident_buf[ATA_IDENT_MODEL + i + 1];
        model_str[i + 1] = ident_buf[ATA_IDENT_MODEL + i];
    }

    if (cmd_sets & (1 << 26)) {
        disk_size = *((uint32_t *) (ident_buf + ATA_IDENT_MAX_LBAEXT));
    } else {
        disk_size = *((uint32_t *) (ident_buf + ATA_IDENT_MAX_LBA));
    }

    kdebug("Model: %s\n", model_str);
    kdebug("Size: %S\n", disk_size * 512);
}

static void ahci_sata_port_init(struct ahci_port_registers *port) {
    ahci_port_stop(port);

    uintptr_t page0 = amd64_phys_alloc_page();
    _assert(page0 != MM_NADDR);
    kdebug("Command list/recv FIS: %p\n", page0);

    memset((void *) MM_VIRTUALIZE(page0), 0, 4096);

    port->p_clb = page0 & 0xFFFFFFFF;
    port->p_clbu = page0 >> 32;

    port->p_fb = (page0 + 0x400) & 0xFFFFFFFF;
    port->p_fbu = (page0 + 0x400) >> 32;

    // 8K
    uintptr_t page1 = amd64_phys_alloc_contiguous(2);
    _assert(page1 != MM_NADDR);
    memset((void *) MM_VIRTUALIZE(page1), 0, 8192);

    struct ahci_command_header *cmd_list = (struct ahci_command_header *) MM_VIRTUALIZE(page0);
    for (size_t i = 0; i < 32; ++i) {
        uintptr_t addr = page1 + i * 256;

        cmd_list[i].prdtl = 8;
        cmd_list[i].ctba = addr & 0xFFFFFFFF;
        cmd_list[i].ctbau = addr >> 32;
    }

    ahci_port_start(port);

    // TODO: add the device somewhere to kernel as /dev/sdX
    //       guess I should implement device management eventually
}

// Setup a SATA port
void ahci_port_init(uint8_t n, struct ahci_port_registers *port) {
    switch (port->p_sig) {
    case AHCI_PORT_SIG_SATA:
        ahci_sata_port_init(port);
        ahci_sata_port_identify(port);
        _assert(ahci_add_port_dev(port) == 0);

        // Enable IRQs
        port->p_ie |= (1 << 0);
        break;
    default:
        kdebug("Skipping unknown drive type\n");
        break;
    }
}

void ahci_port_stop(struct ahci_port_registers *port) {
    port->p_cmd &= ~(AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE);

    while (port->p_cmd & (AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FR));
}

void ahci_port_start(struct ahci_port_registers *port) {
    while (port->p_cmd & AHCI_PORT_CMD_CR);

    port->p_cmd |= AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST;
}

void ahci_init(struct ahci_registers *regs) {
    // Read AHCI version
    kdebug("Initializing AHCI controller version %d.%d\n", regs->vs >> 16, regs->vs & 0xFFFF);

    uint32_t pi = regs->pi;

    // Find available ports
    for (size_t i = 0; i < 32; ++i) {
        uint32_t bit = 1 << i;

        if (pi & bit) {
            struct ahci_port_registers *port = &regs->ports[i];
            uint32_t ssts = port->p_ssts;

            uint8_t ipm = (ssts >> 8) & 0x0F;
            uint8_t spd = (ssts >> 4) & 0x0F;
            uint8_t det = ssts & 0x0F;

            if (ipm != AHCI_PORT_SSTS_IPM_ACTIVE || spd == 0 || det != AHCI_PORT_SSTS_DET_OK) {
                // For one of the reasons the device is not available at this port
                continue;
            }

            ahci_port_init(i, port);
        }
    }

    // Enable HBA interrupts
    regs->ghc |= 1 << 1;
}
