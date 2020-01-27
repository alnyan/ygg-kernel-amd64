#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/rtc.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "acpi.h"

struct acpi_madt *acpi_madt = NULL;
struct acpi_fadt *acpi_fadt = NULL;
struct acpi_mcfg *acpi_mcfg = NULL;

static uint8_t checksum(const void *ptr, size_t size) {
    uint8_t v = 0;
    for (size_t i = 0; i < size; ++i) {
        v += ((const uint8_t *) ptr)[i];
    }
    return v;
}

static ACPI_STATUS acpica_init(void) {
    ACPI_STATUS ret;
    ACPI_OBJECT arg1;
    ACPI_OBJECT_LIST args;

    /*
    * 0 = PIC
    * 1 = APIC
    * 2 = SAPIC ?
    */
    arg1.Type = ACPI_TYPE_INTEGER;
    arg1.Integer.Value = 1;
    args.Count = 1;
    args.Pointer = &arg1;

    if (ACPI_FAILURE(ret = AcpiInitializeSubsystem())) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInitializeTables(NULL, 0, FALSE))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiLoadTables())) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiEvaluateObject(NULL, "\\_PIC", &args, NULL))) {
        if (ret != AE_NOT_FOUND) {
            kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
            return ret;
        }
        kwarn("\\_PIC = AE_NOT_FOUND\n");
        // Guess that's ok?
    }

    if (ACPI_FAILURE(ret = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    return AE_OK;
}

void amd64_acpi_init(void) {
    struct acpi_rsdp_ext *rsdp = NULL;
    struct acpi_rsdt *rsdt;
    acpi_madt = NULL;

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

            acpi_madt = (struct acpi_madt *) hdr;
        } else if (!strncmp((const char *) hdr, "FACP", 4)) {
            kdebug("Found FADT = %p\n", entry_addr);
            if (checksum(hdr, hdr->length) != 0) {
                kdebug("Entry is invalid\n");
                while (1);
            }

            acpi_fadt = (struct acpi_fadt *) hdr;
        } else if (!strncmp((const char *) hdr, "HPET", 4)) {
            kdebug("Found HPET = %p\n", entry_addr);
            if (checksum(hdr, hdr->length) != 0) {
                kdebug("Entry is invalid\n");
                while (1);
            }
        } else if (!strncmp((const char *) hdr, "MCFG", 4)) {
            kdebug("Found MCFG = %p\n", entry_addr);
            if (checksum(hdr, hdr->length) != 0) {
                panic("Entry is invalid\n");
            }

            acpi_mcfg = (struct acpi_mcfg *) hdr;
        }
    }

    if (acpi_fadt) {
        rtc_set_century(acpi_fadt->century);
    }

    if (!acpi_madt) {
        panic("ACPI: No MADT present\n");
    }

    if (ACPI_FAILURE(acpica_init())) {
        panic("Failed to initialize ACPICA\n");
    }
}
