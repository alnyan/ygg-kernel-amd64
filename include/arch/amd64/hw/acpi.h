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

struct acpi_mcfg {
    struct acpi_header hdr;

    uint64_t __res0;
    struct acpi_mcfg_entry {
        uint64_t base_address;
        uint16_t pci_segment_group;
        uint8_t start_pci_bus;
        uint8_t end_pci_bus;
        uint32_t reserved;
    } __attribute__((packed)) entry[];
} __attribute__((packed));

struct acpi_fadt {
    struct acpi_header hdr;

    uint32_t firmware_ctrl;
    uint32_t dsdt;

    uint8_t __res0;

    uint8_t preferred_pm_profile;
    uint16_t sci_int;
    uint32_t sci_cmd_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    //...
} __attribute__((packed));

extern struct acpi_rsdp_ext *acpi_rsdp;
extern struct acpi_madt *acpi_madt;
extern struct acpi_mcfg *acpi_mcfg;

void amd64_acpi_set_rsdp(uintptr_t addr);
void amd64_acpi_set_rsdp2(uintptr_t addr);
void amd64_acpi_init(void);
