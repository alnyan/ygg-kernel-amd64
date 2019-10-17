#include "acpi.h"
#include "sys/debug.h"

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context) {
    kwarn("ACPI ISR INSTALL STUB\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine) {
    kwarn("ACPI ISR REMOVE STUB\n");
    return AE_OK;
}
