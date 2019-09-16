#pragma once
#include <stdint.h>

struct hpet_timer_block {
    uint64_t timer_cfgr;
    uint64_t timer_cmpr;
    uint64_t timer_intr;
    uint64_t __res0;
};

struct hpet {
    uint64_t hpet_caps;
    uint64_t __res0;
    uint64_t hpet_cfgr;
    uint64_t __res1;
    uint64_t hpet_isr;
    char __res2[200];
    uint64_t hpet_cntr;
    uint64_t __res3;
    struct hpet_timer_block timers[32];
};

struct hpet *acpi_hpet_get(void);
