#pragma once
#include "sys/amd64/hw/acpi.h"

void amd64_apic_init(struct acpi_madt *madt);
