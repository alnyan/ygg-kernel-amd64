#include "acpi.h"
#include "sys/panic.h"

ACPI_THREAD_ID AcpiOsGetThreadId(void) {
    return 1;
}

void AcpiOsSleep(UINT64 Milliseconds) {}

void AcpiOsStall(UINT32 Microseconds) {}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context) {
    panic("AcpiOsExecute: stub\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) {
    *OutHandle = 0;
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) {
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) {
}

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle) {
    *OutHandle = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout) {
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
    return AE_OK;
}
