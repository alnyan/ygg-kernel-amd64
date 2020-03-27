#pragma once
#include "sys/types.h"

#define IOC_GETVMODE        0x8001
#define IOC_FBCON           0x8002

struct ioc_vmode {
    uint32_t width;
    uint32_t height;
};
