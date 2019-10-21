#pragma once

#define ATA_REG_DATA            0x00
#define ATA_REG_ERROR           0x01
#define ATA_REG_FEATURES        0x01
#define ATA_REG_SECCOUNT0       0x02
#define ATA_REG_LBA0            0x03
#define ATA_REG_LBA1            0x04
#define ATA_REG_LBA2            0x05
#define ATA_REG_HDDEVSEL        0x06
#define ATA_REG_COMMAND         0x07
#define ATA_REG_STATUS          0x07
#define ATA_REG_SECCOUNT1       0x08
#define ATA_REG_LBA3            0x09
#define ATA_REG_LBA4            0x0A
#define ATA_REG_LBA5            0x0B
#define ATA_REG_CONTROL         0x0C
#define ATA_REG_ALTSTATUS       0x0C
#define ATA_REG_DEVADDRESS      0x0D

#define ATA_CONTROL_NO_INT      (1 << 1)

#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_READ_DMA_EX     0x25

#define ATA_SR_BUSY             (1 << 7)
#define ATA_SR_DRQ              (1 << 3)
#define ATA_SR_ERR              (1 << 0)

#define ATA_IDENT_DEVICE_TYPE   0x00
#define ATA_IDENT_CYLINDERS     0x02
#define ATA_IDENT_HEADS         0x06
#define ATA_IDENT_SECTORS       0x0C
#define ATA_IDENT_SERIAL        0x14
#define ATA_IDENT_MODEL         0x36
#define ATA_IDENT_CAPS          0x62
#define ATA_IDENT_MAX_LBA       0x78
#define ATA_IDENT_CMD_SETS      0xA4
#define ATA_IDENT_MAX_LBAEXT    0xC8
