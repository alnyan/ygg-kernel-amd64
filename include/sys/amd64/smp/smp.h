#pragma once
#include "sys/types.h"

struct cpu {
    struct cpu *self;
    uint64_t flags;

    uint64_t processor_id;
    uint64_t apic_id;
};

void amd64_smp_add(uint8_t apic_id);
void amd64_smp_init(void);
void amd64_load_ap_code(void);
