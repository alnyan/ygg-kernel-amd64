#include "acpi.h"
#include "drivers/pci/pci.h"
#include "sys/panic.h"
#include "arch/amd64/hw/io.h"
#include "sys/assert.h"
#include "sys/debug.h"

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width) {
    switch (Width) {
    case 8:
        *Value = inb(Address);
        return AE_OK;
    case 16:
        *Value = inw(Address);
        return AE_OK;
    case 32:
        *Value = inl(Address);
        return AE_OK;
    default:
        panic("Unsupported port read width: %u\n", Width);
    }
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
    switch (Width) {
    case 8:
        outb(Address, Value & 0xFF);
        return AE_OK;
    case 16:
        outw(Address, Value & 0xFFFF);
        return AE_OK;
    case 32:
        outl(Address, Value);
        return AE_OK;
    default:
        panic("Unsupported port write width: %u\n", Width);
    }
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width) {
    UINT32 DWordAlignedReg = Reg & ~3;
    UINT32 DWord;

    DWord = pci_config_read_dword_legacy(PciId->Bus, PciId->Device, PciId->Function, DWordAlignedReg);

    switch (Width) {
    case 8:
        DWord >>= Reg - DWordAlignedReg;
        *Value = DWord & 0xFF;
        return AE_OK;
    case 16:
        if (DWord != DWordAlignedReg) {
            DWord >>= 16;
        }
        *Value = DWord & 0xFFFF;
        return AE_OK;
    case 32:
        _assert(DWordAlignedReg == Reg);
        *Value = DWord;
        return AE_OK;
    default:
        panic("Unsupported PCI read width: %u\n", Width);
    }
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width) {
    panic("Stub\n");
    return AE_OK;
}

UINT64 AcpiOsGetTimer(void) {
    return system_time;
}
