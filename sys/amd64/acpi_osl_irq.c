#include "acpi.h"
#include "sys/amd64/hw/irq.h"
#include "sys/debug.h"

static ACPI_OSD_HANDLER sci_wrapped;

uint32_t sci_wrapper(void *ctx) {
    kdebug("!!! SCI WRAPPER !!!\n");
    return sci_wrapped(ctx);
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context) {
    kwarn("ACPI ISR INSTALL STUB\n");
    sci_wrapped = ServiceRoutine;
    irq_add_leg_handler(InterruptNumber, sci_wrapper, Context);
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine) {
    kwarn("ACPI ISR REMOVE STUB\n");
    return AE_OK;
}
