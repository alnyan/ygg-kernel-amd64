#include "sys/amd64/hw/pci/pci.h"
#include "sys/debug.h"
#include "sys/mm.h"

#define AHCI_PORT_SSTS_DET_OK               0x3
#define AHCI_PORT_SSTS_IPM_ACTIVE           0x1
#define AHCI_PORT_SIG_SATA                  0x00000101
#define AHCI_PORT_SIG_SATAPI                0xEB140101

enum ahci_fis_type {
    FIS_REG_H2D         = 0x27,
    FIS_REG_D2H         = 0x34,
    FIS_DMA_ACT         = 0x39,
    FIS_DMA_SETUP       = 0x41,
    FIS_DATA            = 0x46,
    FIS_BIST            = 0x58,
    FIS_PIO_SETUP       = 0x5F,
    FIS_DEV_BITS        = 0xA1
};

struct ahci_registers {
    // Generic host control
    uint32_t cap;           // 0x00
    uint32_t ghc;           // 0x04
    uint32_t is;            // 0x08
    uint32_t pi;            // 0x0C
    uint32_t vs;            // 0x10
    uint32_t ccc_ctl;       // 0x14
    uint32_t ccc_ports;     // 0x18
    uint32_t em_loc;        // 0x1C
    uint32_t em_ctl;        // 0x20
    uint32_t cap2;          // 0x24
    uint32_t bohc;          // 0x28
    // Reserved
    uint32_t __res0[13];
    // Reserved for NVMHCI
    // Also vendor specific settings
    uint32_t __res1[40];

    // Port control registers
    struct ahci_port_registers {
        uint32_t p_clb;
        uint32_t p_clbu;
        uint32_t p_fb;
        uint32_t p_fbu;
        uint32_t p_is;
        uint32_t p_ie;
        uint32_t p_cmd;
        uint32_t __res0;
        uint32_t p_tfd;
        uint32_t p_sig;
        uint32_t p_ssts;
        uint32_t p_sctl;
        uint32_t p_serr;
        uint32_t p_sact;
        uint32_t p_ci;
        uint32_t p_sntf;
        uint32_t p_fbs;
        uint32_t p_devslp;
        uint32_t __res1[14];
    } __attribute__((packed)) ports[32];
} __attribute__((packed));

struct ahci_fis_reg_h2d {
    uint8_t type;
    uint8_t cmd_port;
    uint8_t cmd;
    uint8_t feature_low;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t dev;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t feature_high;

    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;

    uint8_t __res0[4];
};

struct ahci_fis_reg_d2h {
    uint8_t type;
    uint8_t cmd_port;
    uint8_t status;
    uint8_t error;

    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t dev;

    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t __res0;

    uint8_t countl;
    uint8_t counth;

    uint8_t __res1[6];
};

struct pci_ahci {
    pci_addr_t addr;
    uint32_t abar_phys;
    struct ahci_registers *volatile regs;
};

// Only one controller is supported now
static struct pci_ahci ahci;

// Setup a SATA port
static void pci_ahci_sata_add(uint8_t n, struct ahci_port_registers *port) {
    kdebug("SATA port %d\n", n);
}

// SATAPI port
static void pci_ahci_satapi_add(uint8_t n, struct ahci_port_registers *port) {
    kdebug("SATAPI port %d\n", n);
}

void pci_ahci_init(pci_addr_t addr) {
    kdebug("Initializing AHCI controller at " PCI_FMTADDR "\n", PCI_VAADDR(addr));

    ahci.addr = addr;
    ahci.abar_phys = pci_config_read_dword(addr, 0x24);
    ahci.regs = (struct ahci_registers *) MM_VIRTUALIZE(ahci.abar_phys);

    kdebug("AHCI registers: %p\n", ahci.regs);

    // Read AHCI version
    kdebug("AHCI controller version %d.%d\n", ahci.regs->vs >> 16, ahci.regs->vs & 0xFFFF);

    uint32_t pi = ahci.regs->pi;

    // Find available ports
    for (size_t i = 0; i < 32; ++i) {
        uint32_t bit = 1 << i;

        if (pi & bit) {
            struct ahci_port_registers *port = &ahci.regs->ports[i];
            uint32_t ssts = port->p_ssts;

            uint8_t ipm = (ssts >> 8) & 0x0F;
            uint8_t spd = (ssts >> 4) & 0x0F;
            uint8_t det = ssts & 0x0F;

            if (ipm != AHCI_PORT_SSTS_IPM_ACTIVE || spd == 0 || det != AHCI_PORT_SSTS_DET_OK) {
                // For one of the reasons the device is not available at this port
                continue;
            }

            switch (port->p_sig) {
            case AHCI_PORT_SIG_SATA:
                pci_ahci_sata_add(i, port);
                break;
            case AHCI_PORT_SIG_SATAPI:
                pci_ahci_satapi_add(i, port);
                break;
            }
        }
    }
}
