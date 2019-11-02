#pragma once
#include "sys/types.h"

struct netdev;

typedef int (*netdev_tx_func_t) (struct netdev *, const void *, size_t);

struct netdev {
    uint8_t hwaddr[6];
    char name[32];
    netdev_tx_func_t tx;
};
