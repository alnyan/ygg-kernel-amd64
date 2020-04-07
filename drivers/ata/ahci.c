#include "drivers/ata/ahci.h"
#include "drivers/pci/pci.h"
#include "drivers/ata/ata.h"
#include "sys/mem/phys.h"
#include "user/errno.h"
#include "sys/block/blk.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "sys/dev.h"
#include "sys/mm.h"

#define AHCI_SPIN_WAIT_MAX              10000000
#define AHCI_PRD_MAX_SIZE               8192

#define AHCI_PORT_SIG_SATA              0x00000101
#define AHCI_PORT_SIG_SATAPI            0xEB140101

#define AHCI_MAX_PORTS                  32
#define AHCI_CMD_LIST_SIZE              32
#define AHCI_CMD_TABLE_ENTSZ            256

#define AHCI_PORT_IS_TFES               (1 << 30)

#define AHCI_PORT_SSTS_IPM_ACTIVE       0x01
#define AHCI_PORT_SSTS_DET_CONNECTED    0x03

#define AHCI_FIS_REG_H2D                0x27
// Tells the controller it's a command FIS
#define AHCI_FIS_REG_H2D_COMMAND        (1 << 7)

#define AHCI_PORT_CMD_LIST(port)    \
    ((struct ahci_cmd_list *) MM_VIRTUALIZE((port)->clb | (((uintptr_t) (port)->clbu) << 32)))
#define AHCI_PORT_TABLE_ENTRY(list, n)  \
    ((struct ahci_cmd_table *) MM_VIRTUALIZE((list)[n].ctba))

// Start
#define AHCI_PORT_CMD_ST                (1 << 0)
// FIS receive enable
#define AHCI_PORT_CMD_FRE               (1 << 4)
// FIS receive running
#define AHCI_PORT_CMD_FR                (1 << 14)
// Command list running
#define AHCI_PORT_CMD_CR                (1 << 15)

struct ahci_port {
    uint32_t clb;       // 0x00
    uint32_t clbu;      // 0x04
    uint32_t fb;        // 0x08
    uint32_t fbu;       // 0x0C
    uint32_t is;        // 0x10
    uint32_t ie;        // 0x14
    uint32_t cmd;       // 0x18
    uint32_t __res0;    // 0x1C
    uint32_t tfd;       // 0x20
    uint32_t sig;       // 0x24
    uint32_t ssts;      // 0x28
    uint32_t sctl;      // 0x2C
    uint32_t serr;      // 0x30
    uint32_t sact;      // 0x34
    uint32_t ci;        // 0x38
    uint32_t __res1[13];// 0x3C
    uint32_t __vnd[4];
};

struct ahci_registers {
    // 0x00 - 0x1F - Generic Host Control
    uint32_t cap;       // 0x00
    uint32_t ghc;       // 0x04
    uint32_t is;        // 0x08
    uint32_t pi;        // 0x0C
    uint32_t vs;        // 0x10
    uint32_t __unspec[3];
    // 0x20 - 0x9F - Reserved
    uint32_t __res[32];
    // 0xA0 - 0xFF - Vendor specific registers
    uint32_t __vnd[24];
    struct ahci_port port[32];
};

// Command list entry
struct ahci_cmd_list {
    uint16_t attr;
    // PRD table length in entries
    uint16_t prdtl;
    // Byte count transferred
    volatile uint32_t prdbc;
    // Command table base address
    uint64_t ctba;
    uint32_t __res0[4];
};

// Physical region descriptor entry
struct ahci_prd {
    // Buffer address
    uint64_t dba;
    uint32_t __res0;
    uint32_t dbc;
};

// Command table entry
struct ahci_cmd_table {
    // Buffer for FIS to be sent to the device
    union {
        struct ahci_fis_reg_h2d fis_reg_h2d;
        uint8_t __cmd_fis[64];
    };
    uint8_t acmd[16];
    uint8_t __res0[48];
    // Any number of entries can follow
    struct ahci_prd prdt[0];
};

//// Generic AHCI

static void ahci_port_stop(struct ahci_port *port) {
    // Disable command engine and FIS receive
    port->cmd &= ~(AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE);

    // Wait for controller to change status
    while (port->cmd & (AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FR)) {
        asm ("pause");
    }
}

static void ahci_port_start(struct ahci_port *port) {
    // Wait while port may be processing any commands
    while (port->cmd & AHCI_PORT_CMD_CR) {
        asm ("pause");
    }

    // Enable command engine and FIS receive
    port->cmd |= AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST;
}

static int ahci_port_cmd_alloc(struct ahci_port *port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < AHCI_CMD_LIST_SIZE; ++i) {
        if (!(slots & (1U << i))) {
            return i;
        }
    }
    return -1;
}

static int ahci_port_alloc(struct ahci_port *port) {
    // Disable port command engine first
    ahci_port_stop(port);

    // AHCI port memory:
    //  * 1024 for command list
    //  * 256 - FIS receive buffer
    //  * 8192 - command table
    // Total:
    // 12288 bytes (page-aligned), 3 pages

    // Command list and FIS buffer
    uintptr_t page0 = mm_phys_alloc_page(); //amd64_phys_alloc_page();
    if (page0 == MM_NADDR) {
        kerror("Failed to allocate a page\n");
        return -ENOMEM;
    }

    // Command table
    uintptr_t page1 = mm_phys_alloc_contiguous(2); //amd64_phys_alloc_contiguous(2);
    if (page1 == MM_NADDR) {
        kerror("Failed to allocate 2 pages\n");
        mm_phys_free_page(page0);
        //amd64_phys_free(page0);
        return -ENOMEM;
    }

    // Cleanup the pages
    memset((void *) MM_VIRTUALIZE(page0), 0, 0x1000);
    memset((void *) MM_VIRTUALIZE(page1), 0, 0x2000);

    // Setup an empty command list with proper addresses
    struct ahci_cmd_list *cmd_list = (struct ahci_cmd_list *) MM_VIRTUALIZE(page0);
    for (size_t i = 0; i < AHCI_CMD_LIST_SIZE; ++i) {
        cmd_list[i].prdtl = 8;
        cmd_list[i].ctba = page1 + i * AHCI_CMD_TABLE_ENTSZ;
    }

    // Configure the port to use allocated pages
    port->clb = page0 & 0xFFFFFFFF;
    port->clbu = page0 >> 32;
    port->fb = (page0 + 0x400) & 0xFFFFFFFF;
    port->fbu = (page0 + 0x400) >> 32;

    // Enable it again
    ahci_port_start(port);
    return 0;
}

static int ahci_port_ata_cmd(struct ahci_port *port, uint8_t ata, uintptr_t lba, void *data, size_t len) {
    // At least should be sector-aligned
    _assert(len % 512 == 0);

    int cmd = ahci_port_cmd_alloc(port);
    if (cmd < 0) {
        return -EBUSY;
    }

    struct ahci_cmd_list *list = AHCI_PORT_CMD_LIST(port);
    struct ahci_cmd_list *list_entry = &list[cmd];
    struct ahci_cmd_table *table_entry = AHCI_PORT_TABLE_ENTRY(list, cmd);
    uint32_t spin = 0;

    // TODO: support devices with different sector sizes
    size_t nsect = (len + 511) / 512;
    _assert((nsect & ~0xFFFF) == 0);
    size_t prd_count = (len + AHCI_PRD_MAX_SIZE - 1) / AHCI_PRD_MAX_SIZE;
    kdebug("Command requires %d PRDs\n", prd_count);

    // Setup command table entry
    memset(table_entry, 0, sizeof(struct ahci_cmd_table) + sizeof(struct ahci_prd) * prd_count);
    for (size_t i = 0, bytes_left = len; i < prd_count; ++i) {
        size_t prd_size = MIN(AHCI_PRD_MAX_SIZE, len);
        _assert(prd_size);

        uintptr_t prd_buf_phys = MM_PHYS(data) + i * AHCI_PRD_MAX_SIZE;

        table_entry->prdt[i].dba = prd_buf_phys;
        table_entry->prdt[i].dbc = ((prd_size - 1) << 1) | 1;

        kdebug("PRD%d: %S at %p\n", i, prd_size, prd_buf_phys);

        if (i == prd_count - 1) {
            // Mark last PRDT entry
            table_entry->prdt[i].dbc |= 1U << 31;
        }

        bytes_left -= prd_size;
    }

    // Setup command list entry
    // attr = FIS size in dwords
    list_entry->attr = sizeof(struct ahci_fis_reg_h2d) / sizeof(uint32_t);
    list_entry->prdtl = prd_count;
    list_entry->prdbc = 0;

    // Setup FIS packet
    struct ahci_fis_reg_h2d *fis = &table_entry->fis_reg_h2d;
    memset(fis, 0, sizeof(struct ahci_fis_reg_h2d));
    fis->type = AHCI_FIS_REG_H2D;
    fis->cmd = ata;
    fis->cmd_port = AHCI_FIS_REG_H2D_COMMAND;
    if (ata != ATA_CMD_IDENTIFY) {
        fis->device = 1 << 6; // LBA mode
        fis->lba0 = lba & 0xFF;
        fis->lba1 = (lba >> 8) & 0xFF;
        fis->lba2 = (lba >> 16) & 0xFF;
        fis->lba3 = (lba >> 24) & 0xFF;
        fis->lba4 = (lba >> 32) & 0xFF;
        fis->lba5 = (lba >> 40) & 0xFF;
        kdebug("nsect = %d\n", nsect);
        fis->count = nsect & 0xFFFF;
    }

    // Wait for port to be free for commands
    while ((port->tfd & (ATA_SR_BUSY | ATA_SR_DRQ)) && spin < AHCI_SPIN_WAIT_MAX) {
        asm volatile ("pause");
        ++spin;
    }

    if (spin == AHCI_SPIN_WAIT_MAX) {
        kerror("AHCI device hang\n");
        return -EIO;
    }

    // Request HBA to execute the command
    port->ci |= 1 << cmd;

    while (1) {
        asm volatile ("nop");

        if (!(port->ci & (1 << cmd))) {
            break;
        }

        if (port->is & AHCI_PORT_IS_TFES) {
            kerror("Device signalled TFE error\n");
            return -EIO;
        }
    }

    if (port->is & AHCI_PORT_IS_TFES) {
        kerror("Device signalled TFE error\n");
        return -EIO;
    }

    return 0;
}

static int ahci_port_atapi_read(struct ahci_port *port, uint64_t lba, void *buf, size_t nsect) {
    int cmd = ahci_port_cmd_alloc(port);
    if (cmd < 0) {
        return -EBUSY;
    }

    struct ahci_cmd_list *list = AHCI_PORT_CMD_LIST(port);
    struct ahci_cmd_list *list_entry = &list[cmd];
    struct ahci_cmd_table *table_entry = AHCI_PORT_TABLE_ENTRY(list, cmd);
    uint32_t spin = 0;

    // TODO: support devices with different sector sizes
    size_t len = nsect * 2048;
    _assert((nsect & ~0xFFFF) == 0);
    size_t prd_count = (len + AHCI_PRD_MAX_SIZE - 1) / AHCI_PRD_MAX_SIZE;
    kdebug("Command requires %d PRDs\n", prd_count);

    // Setup command table entry
    memset(table_entry, 0, sizeof(struct ahci_cmd_table) + sizeof(struct ahci_prd) * prd_count);
    table_entry->acmd[0] = 0xA8;
    table_entry->acmd[9] = nsect;
    table_entry->acmd[5] = lba & 0xFF;
    table_entry->acmd[4] = (lba >> 8) & 0xFF;
    table_entry->acmd[3] = (lba >> 16) & 0xFF;
    table_entry->acmd[2] = (lba >> 24) & 0xFF;

    for (size_t i = 0, bytes_left = len; i < prd_count; ++i) {
        size_t prd_size = MIN(AHCI_PRD_MAX_SIZE, len);
        _assert(prd_size);

        uintptr_t prd_buf_phys = MM_PHYS(buf) + i * AHCI_PRD_MAX_SIZE;

        table_entry->prdt[i].dba = prd_buf_phys;
        table_entry->prdt[i].dbc = ((prd_size - 1) << 1) | 1;

        kdebug("PRD%d: %S at %p\n", i, prd_size, prd_buf_phys);

        if (i == prd_count - 1) {
            // Mark last PRDT entry
            table_entry->prdt[i].dbc |= 1 << 31;
        }

        bytes_left -= prd_size;
    }

    // Setup command list entry
    // attr = FIS size in dwords
    list_entry->attr = sizeof(struct ahci_fis_reg_h2d) / sizeof(uint32_t);
    // This is ATAPI command
    list_entry->attr |= (1 << 5);
    list_entry->prdtl = 1;
    list_entry->prdbc = 0;

    // Setup FIS packet
    struct ahci_fis_reg_h2d *fis = &table_entry->fis_reg_h2d;
    memset(fis, 0, sizeof(struct ahci_fis_reg_h2d));
    fis->type = AHCI_FIS_REG_H2D;
    fis->cmd = ATA_CMD_PACKET;
    fis->cmd_port = AHCI_FIS_REG_H2D_COMMAND;

    fis->device = 1 << 6; // LBA mode
    fis->feature_low = 1;
    fis->lba0 = lba & 0xFF;
    fis->lba1 = (lba >> 8) & 0xFF;
    fis->lba2 = (lba >> 16) & 0xFF;
    fis->lba3 = (lba >> 24) & 0xFF;
    fis->lba4 = (lba >> 32) & 0xFF;
    fis->lba5 = (lba >> 40) & 0xFF;
    fis->count = nsect & 0xFFFF;

    // Wait for port to be free for commands
    while ((port->tfd & (ATA_SR_BUSY | ATA_SR_DRQ)) && spin < AHCI_SPIN_WAIT_MAX) {
        ++spin;
    }

    if (spin == AHCI_SPIN_WAIT_MAX) {
        kerror("AHCI device hang\n");
        return -EIO;
    }

    // Request HBA to execute the command
    port->ci |= 1 << cmd;

    while (1) {
        if (!(port->ci & (1 << cmd))) {
            break;
        }

        if (port->is & AHCI_PORT_IS_TFES) {
            kerror("Device signalled TFE error\n");
            return -EIO;
        }
    }

    if (port->is & AHCI_PORT_IS_TFES) {
        kerror("Device signalled TFE error\n");
        return -EIO;
    }

    return 0;
}

static int ahci_port_identify(struct ahci_port *port) {
    char ident_buf[1024];
    size_t disk_size_lba;
    char model_string[41];
    char serial_string[11];
    uint32_t cmd_sets;
    int res, id_cmd;

    if (port->sig == AHCI_PORT_SIG_SATA) {
        id_cmd = ATA_CMD_IDENTIFY;
    } else {
        id_cmd = ATA_CMD_PACKET_IDENTIFY;
    }

    if ((res = ahci_port_ata_cmd(port, id_cmd, 0, ident_buf, sizeof(ident_buf))) != 0) {
        kwarn("Disk identify failed: %s\n", kstrerror(res));
        return res;
    }

    model_string[40] = 0;
    serial_string[10] = 0;
    cmd_sets = *(uint32_t *) (ident_buf + ATA_IDENT_CMD_SETS);

    for (size_t i = 0; i < 40; i += 2) {
        model_string[i] = ident_buf[ATA_IDENT_MODEL + i + 1];
        model_string[i + 1] = ident_buf[ATA_IDENT_MODEL + i];
    }
    for (size_t i = 39; i > 0; --i) {
        if (model_string[i] != ' ') {
            break;
        }
        model_string[i] = 0;
    }

    for (size_t i = 0; i < 10; i += 2) {
        serial_string[i] = ident_buf[ATA_IDENT_SERIAL + i + 1];
        serial_string[i + 1] = ident_buf[ATA_IDENT_SERIAL + i];
    }
    for (size_t i = 9; i > 0; --i) {
        if (serial_string[i] != ' ') {
            break;
        }
        serial_string[i] = 0;
    }

    if (cmd_sets & (1 << 26)) {
        disk_size_lba = *(uint32_t *) (ident_buf + ATA_IDENT_MAX_LBAEXT);
    } else {
        disk_size_lba = *(uint32_t *) (ident_buf + ATA_IDENT_MAX_LBA);
    }

    kdebug("%s drive: \"%s\", Serial: \"%s\", Capacity: %S\n",
            id_cmd == ATA_CMD_IDENTIFY ? "SATA" : "SATAPI",
            model_string, serial_string, disk_size_lba * 512);

    return 0;
}

static ssize_t ahci_blk_write(struct blkdev *blk, const void *buf, size_t off, size_t len) {
    if ((off % blk->block_size) != 0) {
        panic("Misaligned AHCI device write: offset of %lu, block size is %lu\n", off, blk->block_size);
    }
    if ((len % blk->block_size) != 0) {
        panic("Misaligned AHCI device write: size of %lu\n", len);
    }

    kdebug("AHCI write %S at %p\n", len, off);
    struct ahci_port *port = blk->dev_data;
    _assert(port);
    uintptr_t lba = (off / blk->block_size);
    size_t n_blocks = len / blk->block_size;
    int res;

    switch (port->sig) {
    case AHCI_PORT_SIG_SATA:
        if ((res = ahci_port_ata_cmd(port, ATA_CMD_WRITE_DMA_EX, lba, (void *) buf, len)) != 0) {
            return res;
        }
        return len;
    case AHCI_PORT_SIG_SATAPI:
        return -EROFS;
    default:
        return -EIO;
    }
}

static ssize_t ahci_blk_read(struct blkdev *blk, void *buf, size_t off, size_t len) {
    if ((off % blk->block_size) != 0) {
        panic("Misaligned AHCI device read: offset of %lu, block size is %lu\n", off, blk->block_size);
    }
    if ((len % blk->block_size) != 0) {
        panic("Misaligned AHCI device read: size of %lu\n", len);
    }

    kdebug("AHCI read %S at %p\n", len, off);
    struct ahci_port *port = blk->dev_data;
    _assert(port);
    uintptr_t lba = (off / blk->block_size);
    size_t n_blocks = len / blk->block_size;
    int res;

    switch (port->sig) {
    case AHCI_PORT_SIG_SATA:
        if ((res = ahci_port_ata_cmd(port, ATA_CMD_READ_DMA_EX, lba, buf, len)) != 0) {
            return res;
        }
        return len;
    case AHCI_PORT_SIG_SATAPI:
        if ((res = ahci_port_atapi_read(port, lba, buf, n_blocks)) != 0) {
            return res;
        }
        return len;
    default:
        return -EIO;
    }
}

static void ahci_port_add(struct ahci_port *port) {
    struct blkdev *blk = kmalloc(sizeof(struct blkdev));
    _assert(blk);
    uint32_t subclass;

    switch (port->sig) {
    case AHCI_PORT_SIG_SATA:
        blk->block_size = 512;
        subclass = DEV_BLOCK_SDx;
        break;
    case AHCI_PORT_SIG_SATAPI:
        blk->block_size = 2048;
        subclass = DEV_BLOCK_CDx;
        break;
    default:
        panic("Unhandled AHCI device signature: %08x\n", port->sig);
    }

    blk->dev_data = port;
    blk->read = ahci_blk_read;
    blk->write = ahci_blk_write;
    blk->flags = 0;

    _assert(dev_add(DEV_CLASS_BLOCK, subclass, blk, NULL) == 0);
}

static void ahci_port_init(struct ahci_controller *ahci, struct ahci_port *port, int no) {
    kinfo("Initializing port %d\n", no);

    // Allocate all the data structures required for port control
    if (ahci_port_alloc(port) != 0) {
        kwarn("Failed to allocate port data %d\n", no);
        return;
    }

    if (ahci_port_identify(port) != 0) {
        kwarn("Failed to identify device at port %d\n", no);
        return;
    }

    // Register new block device
    ahci_port_add(port);
}

static void ahci_controller_init(struct ahci_controller *ahci) {
    // Check controller version
    kinfo("AHCI controller version is %02x.%02x\n", (ahci->regs->vs >> 16), (ahci->regs->vs & 0xFFFF));

    // Check which ports are supported by AHCI controller
    for (size_t i = 0; i < AHCI_MAX_PORTS; ++i) {
        if (ahci->regs->pi & (1U << i)) {
            // Get SATA connnection status
            struct ahci_port *port = &ahci->regs->port[i];

            uint32_t ssts = port->ssts;
            uint8_t ipm = (ssts >> 8) & 0x0F;
            uint8_t spd = (ssts >> 4) & 0x0F;
            uint8_t det = ssts & 0x0F;

            if ((ipm != AHCI_PORT_SSTS_IPM_ACTIVE) || (det != AHCI_PORT_SSTS_DET_CONNECTED)) {
                // Skip not connected or inactive ports
                continue;
            }

            if (port->sig == AHCI_PORT_SIG_SATA || port->sig == AHCI_PORT_SIG_SATAPI) {
                ahci_port_init(ahci, port, i);
            }
        }
    }
}

//// PCI-specific

static void pci_ahci_init(struct pci_device *pci_dev) {
    // TODO: change pcie -> pci
    uint32_t abar_phys = pci_config_read_dword(pci_dev, PCI_CONFIG_BAR(5));
    if (abar_phys & 1) {
        kwarn("AHCI controller has ABAR in I/O space\n");
        return;
    }

    struct ahci_controller *obj = kmalloc(sizeof(struct ahci_controller));

    obj->pci_dev = pci_dev;
    obj->abar_phys = abar_phys;
    obj->regs = (void *) MM_VIRTUALIZE(abar_phys);

    ahci_controller_init(obj);
}

static __init void ahci_register_class(void) {
    pci_add_class_driver(0x010601, pci_ahci_init, "ahci");
}
