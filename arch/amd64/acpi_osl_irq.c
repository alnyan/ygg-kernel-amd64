#include "acpi.h"
#include "arch/amd64/hw/irq.h"
#include "sys/debug.h"

static ACPI_OSD_HANDLER sci_wrapped;

uint32_t sci_wrapper(void *ctx) {
    return sci_wrapped(ctx);
}

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine, void *Context) {
    sci_wrapped = ServiceRoutine;
    irq_add_leg_handler(InterruptNumber, sci_wrapper, Context);
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER ServiceRoutine) {
    kwarn("ACPI ISR REMOVE STUB\n");
    return AE_OK;
}
