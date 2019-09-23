#pragma once
#include <stdint.h>

typedef struct hpet acpi_hpet_t;
//extern acpi_hpet_t *volatile hpet;

void acpi_hpet_set_base(uintptr_t v);
int acpi_hpet_init(void);
