#pragma once
#include "arch/amd64/hw/acpi.h"
#include "arch/amd64/asm/asm_apic.h"

#define LAPIC(reg) \
    (*(uint32_t *) ((reg) + local_apic))

extern uintptr_t local_apic;

void amd64_apic_set_madt(struct acpi_madt *madt);
void amd64_apic_init(void);
