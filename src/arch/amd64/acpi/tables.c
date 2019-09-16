#include "arch/amd64/acpi/tables.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/mem.h"
#include "sys/mm.h"

#define ACPI_SIGN_RSDP  "RSD PTR "
#define ACPI_SIGN_FADT  "FACP"
#define ACPI_SIGN_APIC  "APIC"
#define ACPI_SIGN_HPET  "HPET"

static const char *acpi_table_type_name[ACPI_TABLE_TYPE_COUNT] = {
    ACPI_SIGN_FADT,
    ACPI_SIGN_APIC,
    ACPI_SIGN_HPET
};
uintptr_t acpi_tables[ACPI_TABLE_TYPE_COUNT] = { MM_NADDR };

static char acpi_checksum(const void *block, size_t count) {
    char r = 0;
    for (size_t i = 0; i < count; ++i) {
        r += ((const char *) block)[i];
    }
    return r;
}

static int acpi_xsdp_read(const struct acpi_rsdp_extended *xsdp) {
    panic("Not yet implemented\n");
    return 0;
}

static int acpi_rsdp_read(const struct acpi_rsdp *rsdp) {
    struct acpi_rsdt *rsdt = (struct acpi_rsdt *) MM_VIRTUALIZE(rsdp->rsdt_addr);

    if (strncmp(rsdt->header.signature, "RSDT", 4)) {
        panic("RSDP does not point to a valid RSDT\n");
    }

    if (acpi_checksum(rsdt, rsdt->header.length)) {
        panic("RSDT checksum is invalid\n");
    }

    uint32_t nentr = (rsdt->header.length - sizeof(struct acpi_sdt_header)) / 4;
    kdebug("RSDT contains %u entries\n", nentr);

    // Dump entries
    for (uint32_t i = 0; i < nentr; ++i) {
        struct acpi_sdt_header *header = (struct acpi_sdt_header *) MM_VIRTUALIZE(rsdt->entries[i]);
        // Find out the type of the table
        for (size_t j = 0; j < ACPI_TABLE_TYPE_COUNT; ++j) {
            if (!strncmp(header->signature, acpi_table_type_name[j], 4)) {
                kdebug("%u: %s\n", i, acpi_table_type_name[j]);
                acpi_tables[j] = (uintptr_t) header;
                continue;
            }
        }
        // Ignore the table otherwise - it's unknown to us
    }

    // Must have FADT table
    if (acpi_tables[ACPI_FADT] == MM_NADDR) {
        panic("RSDT contained no FADT pointer\n");
    }

    return 0;
}

int acpi_tables_init(void) {
    kdebug("Trying to locate ACPI tables\n");

    struct acpi_rsdp *rsdp = (struct acpi_rsdp *) MM_NADDR;
    struct acpi_rsdp_extended *xsdp = (struct acpi_rsdp_extended *) MM_NADDR;

    // 1. Try locating in EBDA
    uintptr_t ebda_base = ((uintptr_t) (*((uint16_t *) MM_VIRTUALIZE(0x040E)))) << 4;
    ebda_base = MM_VIRTUALIZE(ebda_base);
    kdebug("Possible EBDA location: %p\n", ebda_base);

    for (uintptr_t addr = ebda_base & ~0xF; addr < ebda_base + 1024; ++addr) {
        if (!strncmp((const char *) addr, ACPI_SIGN_RSDP, 8)) {
            rsdp = (struct acpi_rsdp *) addr;
            if (acpi_checksum(rsdp, sizeof(struct acpi_rsdp))) {
                rsdp = (struct acpi_rsdp *) MM_NADDR;
                continue;
            }
            kdebug("Found RSDP in EBDA @ %p\n", addr);
            break;
        }
    }

    // 2. Try searching in 0xE0000 - 0xFFFFF
    for (uintptr_t addr = MM_VIRTUALIZE(0xE0000); addr < MM_VIRTUALIZE(0xFFFFF); ++addr) {
        if (!strncmp((const char *) addr, ACPI_SIGN_RSDP, 8)) {
            rsdp = (struct acpi_rsdp *) addr;
            if (acpi_checksum(rsdp, sizeof(struct acpi_rsdp))) {
                rsdp = (struct acpi_rsdp *) MM_NADDR;
                continue;
            }
            kdebug("Found RSDP in BIOS romem @ %p\n", addr);
            break;
        }
    }

    if (rsdp == (struct acpi_rsdp *) MM_NADDR) {
        return -1;
    }

    kdebug("RSDP revision is %d\n", rsdp->rev);
    // Check if RSDP is an extended one
    if (rsdp->rev == 2) {
        xsdp = (struct acpi_rsdp_extended *) rsdp;
        if (acpi_xsdp_read(xsdp) != 0) {
            return -1;
        }
    } else {
        kdebug("Falling back to ACPI 1.0\n");
        if (acpi_rsdp_read(rsdp) != 0) {
            return -1;
        }
    }
    // TODO: FADT does not validate on qemu, neither have I tested ACPI > 1.0

    return 0;
}
