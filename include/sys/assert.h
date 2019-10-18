#pragma once
#include "sys/panic.h"

#define assert(c, m, ...) \
    if (!(c)) \
        panic(m, ##__VA_ARGS__);

#define _assert(c) \
    if (!(c)) \
        panic("Assertion failed: " #c "\n");
