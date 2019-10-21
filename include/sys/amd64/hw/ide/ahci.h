#pragma once
#include "sys/types.h"

#define AHCI_PORT_SSTS_DET_OK               0x3
#define AHCI_PORT_SSTS_IPM_ACTIVE           0x1
#define AHCI_PORT_SIG_SATA                  0x00000101
#define AHCI_PORT_SIG_SATAPI                0xEB140101
#define AHCI_PORT_IS_TFES                   (1 << 30)

#define AHCI_PORT_CMD_ST        (1 << 0)
#define AHCI_PORT_CMD_FRE       (1 << 4)
#define AHCI_PORT_CMD_FR        (1 << 14)
#define AHCI_PORT_CMD_CR        (1 << 15)

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

struct ahci_fis_dma_setup {
    uint8_t type;
    uint8_t cmd_port;
    uint8_t __res0[2];
    uint64_t dma_buffer_id;
    uint32_t __res1;
    uint32_t dma_buffer_offset;
    uint32_t transfer_count;
    uint32_t __res2;
};

struct ahci_fis_pio_setup {
	uint8_t type;
    uint8_t cmd_port;
	uint8_t status;
	uint8_t error;

	uint8_t lba0;
	uint8_t lba1;
	uint8_t lba2;
	uint8_t device;

	uint8_t lba3;
	uint8_t lba4;
	uint8_t lba5;
	uint8_t __res0;

	uint8_t countl;
	uint8_t counth;
	uint8_t __res1;
	uint8_t e_status;

	uint16_t tc;
	uint8_t __res2[2];
};

struct ahci_fis_data {
	uint8_t type;
    uint8_t cmd_port;

	uint8_t __res0[2];

	uint32_t data[];
};

struct ahci_recv_fis {
    union {
        // DMA setup FIS
        struct ahci_fis_dma_setup dsfis;
        char __block0[0x20];
    };
    union {
        // PIO setup FIS
        struct ahci_fis_pio_setup psfis;
        char __block1[0x20];
    };
    union {
        // Register FIS
        struct ahci_fis_reg_d2h rfis;
        char __block2[0x18];
    };
    union {
        // Set device bits FIS
        char __block3[0x2];
    };
    union {
        // Unknown FIS
        char __block4[0x40];
    };
};

struct ahci_command_header {
    // Bits:
    // 0..4     Command length in dwords
    // 5        ATAPI
    uint16_t attr;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t __res0[4];
} __attribute__((packed));

struct ahci_prdt_entry {
    uint32_t dba;
    uint32_t dbau;
    uint32_t __res0;
    // Bits:
    // 0        1
    // 1..21    Data byte count
    // 22..30   Reserved
    // 31       Interrupt
    uint32_t dbc;
} __attribute__((packed));

struct ahci_command_table_entry {
    union {
        struct ahci_fis_reg_h2d fis_reg_h2d;
        uint8_t __cmd_fis[64];
    } __attribute__((packed));
    uint8_t acmd[16];
    uint8_t __res0[48];
    struct ahci_prdt_entry prdt[];
} __attribute__((packed));

#define AHCI_PORT_CMD_LIST(p)   (struct ahci_command_header *) MM_VIRTUALIZE(((uintptr_t) (p)->p_clbu << 32) | ((p)->p_clb))
#define AHCI_CMD_TABLE_ENTRY(l, i) (struct ahci_command_table_entry *) MM_VIRTUALIZE(((uintptr_t) (l)[i].ctbau << 32) | ((l)[i].ctba))

// TODO: accept block device instead of port registers
void ahci_sata_read(struct ahci_port_registers *port, void *buf, uint32_t nsect, uint64_t lba);

void ahci_port_start(struct ahci_port_registers *port);
void ahci_port_stop(struct ahci_port_registers *port);

void ahci_port_init(uint8_t n, struct ahci_port_registers *port);

void ahci_init(struct ahci_registers *regs);
