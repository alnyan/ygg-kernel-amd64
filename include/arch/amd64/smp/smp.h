#pragma once
#include "sys/types.h"

extern size_t smp_ncpus;

void amd64_smp_bsp_configure(void);
void amd64_smp_add(uint8_t apic_id);
void amd64_smp_init(void);
void amd64_load_ap_code(void);
