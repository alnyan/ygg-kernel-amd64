#include "acpi.h"
#include "sys/debug.h"

void AcpiOsPrintf(const char *Format, ...) {
    va_list args;
    va_start(args, Format);
    AcpiOsVprintf(Format, args);
    va_end(args);
}

void AcpiOsVprintf(const char *Format, va_list args) {
    debugfv(DEBUG_DEFAULT, Format, args);
}
