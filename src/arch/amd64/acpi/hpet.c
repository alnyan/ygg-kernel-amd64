#include "arch/amd64/acpi/hpet.h"
#include "arch/amd64/acpi/tables.h"
#include "sys/mm.h"

struct hpet *acpi_hpet_get(void) {
    if (acpi_tables[ACPI_HPET] == MM_NADDR) {
        return 0;
    }

    return (struct hpet *) MM_VIRTUALIZE(acpi_tables[ACPI_HPET]);
}
