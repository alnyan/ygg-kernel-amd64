#include "acpi.h"
#include "sys/mm.h"
#include "sys/heap.h"
#include "sys/panic.h"
#include "sys/debug.h"

void *AcpiOsAllocate(size_t sz) {
    void *buf = kmalloc(sz);
    //kdebug("AcpiOsAllocate %S = %p\n", sz, buf);
    return buf;
}

void AcpiOsFree(void *mem) {
    //kdebug("AcpiOsFree %p\n", mem);
    kfree(mem);
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS phys_addr, UINT64 length) {
    if (phys_addr < 0x100000000) {
        return (void *) MM_VIRTUALIZE(phys_addr);
    }
    return NULL;
}

void AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Size) {
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width) {
    panic("Stub\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width) {
    panic("Stub\n");
    return AE_OK;
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info) {
    panic("Stub\n");
    return AE_OK;
}
