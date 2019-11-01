#pragma once
#include "sys/types.h"

#define MAC_FMT     "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_VA(x)   (x)[0], (x)[1], (x)[2], (x)[3], (x)[4], (x)[5]

#define htons(x) \
    ((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF))
