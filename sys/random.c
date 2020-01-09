#include "sys/random.h"
#include "sys/panic.h"
#include "sys/debug.h"

static int random_ready = 0;
static uint64_t xs64_state;

void random_init(uint64_t s) {
    if (random_ready) {
        kinfo("Random is ready already\n");
    }
    random_ready = 1;
    xs64_state = s;
}

uint64_t random_single(void) {
    if (!random_ready) {
        panic("Random was not ready\n");
    }
    uint64_t x = xs64_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return xs64_state = x;
}
