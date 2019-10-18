#pragma once
#include "sys/types.h"

struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t rev;
    uint32_t rsdt_address;
} __attribute__((packed));

struct acpi_rsdp_ext {
    struct acpi_rsdp rsdp;

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct acpi_header {
    char signature[4];
    uint32_t length;
    uint8_t rev;
    uint8_t checksum;
    char oemid[6];
    char oem_table_id[8];
    uint32_t oem_rev;
    char asl_id[4];
    uint32_t asl_rev;
} __attribute__((packed));

struct acpi_rsdt {
    struct acpi_header hdr;

    uint32_t entry[];
} __attribute__((packed));

struct acpi_madt {
    struct acpi_header hdr;

    uint32_t local_apic_addr;
    uint32_t flags;
    char entry[];
} __attribute__((packed));

extern struct acpi_madt *acpi_madt;

void amd64_acpi_init(void);
