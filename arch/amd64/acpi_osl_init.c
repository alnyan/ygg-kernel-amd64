#include "acpi.h"
#include "sys/panic.h"

ACPI_STATUS AcpiOsInitialize(void) {
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate(void) {
    return AE_OK;
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue) {
    return AE_OK;
}

void AcpiOsWaitEventsComplete(void) {
    panic("Stub\n");
}
