#pragma once
#include <stdint.h>

struct acpi_gas {
    char space_id;
    char reg_bit_width;
    char reg_bit_offset;
    char res;
    uint64_t pointer;
} __attribute__((packed));

struct acpi_rsdp {
    char signature[8];
    char checksum;
    char oemid[6];
    char rev;
    uint32_t rsdt_addr;
};

struct acpi_rsdp_extended {
    struct acpi_rsdp rsdp;
    uint32_t length;
    uint64_t xsdt_addr;
    char checksum2;
    char res[3];
};

struct acpi_sdt_header {
    char signature[4];
    uint32_t length;
    char rev;
    char checksum;
    char oemid[6];
    char oemtableid[8];
    uint32_t oemrev;
    uint32_t creatorid;
    uint32_t creatorrev;
};

struct acpi_rsdt {
    struct acpi_sdt_header header;
    uint32_t entries[1];
};

struct acpi_xsdt {
    struct acpi_sdt_header header;
    uint64_t entries[1];
};

struct acpi_fadt {
    struct acpi_sdt_header header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    char res0;
    char preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    char acpi_enable;
    char acpi_disable;
    char s4bios_req;
    char pstate_cnt;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    char pm1_evt_len;
    char pm1_cnt_len;
    char pm2_cnt_len;
    char pm_tmr_len;
    char gpe0_blk_len;
    char gpe1_blk_len;
    char gpe1_base;
    char cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    char duty_offset;
    char duty_width;
    char day_alrm;
    char mon_alrm;
    char century;
    uint16_t ipac_boot_arch;
    char res1;
    uint32_t flags;
    char reset_reg[12];
    char reset_value;
    uint16_t arm_boot_arch;
    char fadt_minor_version;
    // ...
};

struct acpi_hpet {
    struct acpi_sdt_header header;
    uint32_t event_timer_blk_id;
    struct acpi_gas base_address;
    char hpet_number;
    // ...
} __attribute__((packed));

enum acpi_table_type {
    ACPI_FADT,
    ACPI_APIC,
    ACPI_HPET,
    ACPI_TABLE_TYPE_COUNT
};

////

extern uintptr_t acpi_tables[ACPI_TABLE_TYPE_COUNT];

int acpi_tables_init(void);
