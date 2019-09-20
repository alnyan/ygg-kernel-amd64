#include "arch/amd64/acpi/hpet.h"
#include "arch/amd64/acpi/tables.h"
#include "arch/amd64/hw/timer.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/mm.h"

#define HPET_ENABLE_CNF     (1 << 0)
#define HPET_LEG_RT_CNF     (1 << 1)

#define HPET_Tn_INT_TYPE_CNF    (1 << 1)
#define HPET_Tn_INT_ENB_CNF     (1 << 2)
#define HPET_Tn_TYPE_CNF        (1 << 3)
#define HPET_Tn_PER_INT_CAP     (1 << 4)
#define HPET_Tn_SIZE_CAP        (1 << 5)
#define HPET_Tn_VAL_SET_CNF     (1 << 6)
#define HPET_Tn_32MODE_CNF      (1 << 8)
#define HPET_Tn_INT_ROUTE_OFF   (9)
#define HPET_Tn_FSB_EN_CNF      (1 << 14)
#define HPET_Tn_FSB_INT_DEL_CAP (1 << 15)

#define SYSTICK_FREQ            1000

struct hpet_timer_block {
    uint64_t ctrl;
    uint64_t value;
    uint64_t fsb_int_rt;
    uint64_t res;
};

struct hpet {
    uint64_t caps;
    uint64_t res0;
    uint64_t ctrl;
    uint64_t res1;
    uint64_t intr;
    char res2[200];
    uint64_t count;
    uint64_t res3;
    struct hpet_timer_block timers[2];
};

static uint64_t hpet_freq;
static uint64_t hpet_last;
static uint64_t hpet_systick_res;
static uint64_t systime = 0, systime_prev = 0;
static uintptr_t hpet_base = 0;
static struct hpet *volatile hpet;

void acpi_hpet_set_base(uintptr_t v) {
    hpet_base = v;
}

static void acpi_hpet_tick(void) {
    uint64_t hpet_count = hpet->count;
    systime += (hpet_count - hpet_last) / hpet_systick_res;
    hpet_last = hpet_count;

    // if (systime - systime_prev > SYSTICK_FREQ) {
    //     // Should be 1s tick here
    //     systime_prev = systime;
    // }

    hpet->intr |= 1;
}

int acpi_hpet_init(void) {
    if (!hpet_base) {
        return -1;
    }
    uint64_t addr = 0x7FFFFFFFF000ULL;
    hpet = (struct hpet *) addr;
    mm_map_pages_contiguous(mm_kernel, addr, hpet_base, 1, 0);

    uint64_t hpet_clk = (hpet->caps >> 32) & 0xFFFFFFFF;
    assert(hpet_clk != 0, "HPET does not work correctly\n");
    hpet_freq = 1000000000000000ULL / hpet_clk;
    // TODO: specify desired frequency as function param
    uint32_t desired_freq = SYSTICK_FREQ;

    assert(hpet->timers[0].ctrl & HPET_Tn_PER_INT_CAP, "HPET does not support periodic interrupts\n");

    hpet->ctrl |= HPET_ENABLE_CNF | HPET_LEG_RT_CNF;
    hpet->timers[0].ctrl = HPET_Tn_INT_ENB_CNF | HPET_Tn_VAL_SET_CNF | HPET_Tn_TYPE_CNF;
    hpet->timers[0].value = hpet->count + hpet_freq / desired_freq;
    hpet_last = hpet->count;
    hpet_systick_res = hpet_freq / SYSTICK_FREQ;

    amd64_timer_tick = acpi_hpet_tick;

    return 0;
}
