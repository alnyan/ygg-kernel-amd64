#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/apic.h"
#include "sys/string.h"
#include "sys/debug.h"

static uint8_t checksum(const void *ptr, size_t size) {
    uint8_t v = 0;
    for (size_t i = 0; i < size; ++i) {
        v += ((const uint8_t *) ptr)[i];
    }
    return v;
}

void amd64_acpi_init(void) {
    struct acpi_rsdp_ext *rsdp = NULL;
    struct acpi_madt *madt = NULL;
    struct acpi_rsdt *rsdt;

    for (size_t i = 0xFFFFFF0000000000 + 0x000E0000; i < 0xFFFFFF0000000000 + 0x000FFFFF - 8; ++i) {
        if (!strncmp((const char *) i, "RSD PTR ", 8)) {
            rsdp = (struct acpi_rsdp_ext *) i;
            break;
        }
    }

    if (!rsdp) {
        kdebug("Failed to find rsdp\n");
        while (1);
    }

    kdebug("Found RSDP: %p\n", rsdp);

    if (checksum(rsdp, sizeof(struct acpi_rsdp)) != 0) {
        kdebug("RSDP is invalid\n");
        while (1);
    }

    kdebug("RSDP revision %d\n", rsdp->rsdp.rev);
    rsdt = (struct  acpi_rsdt *) (0xFFFFFF0000000000 + rsdp->rsdp.rsdt_address);
    kdebug("RSDT: %p\n", rsdt);

    if (checksum(rsdt, rsdt->hdr.length) != 0) {
        kdebug("RSDT is invalid\n");
        while (1);
    }

    for (size_t i = 0; i < (rsdt->hdr.length - sizeof(struct acpi_header)) / sizeof(uint32_t); ++i) {
        uintptr_t entry_addr = 0xFFFFFF0000000000 + rsdt->entry[i];
        struct acpi_header *hdr = (struct acpi_header *) entry_addr;

        if (!strncmp((const char *) hdr, "APIC", 4)) {
            kdebug("Found MADT = %p\n", entry_addr);
            if (checksum(hdr, hdr->length) != 0) {
                kdebug("Entry is invalid\n");
                while (1);
            }

            madt = (struct acpi_madt *) hdr;
        }
    }

    if (!madt) {
        kdebug("No MADT\n");
        while (1);
    }

    amd64_apic_init(madt);
}
