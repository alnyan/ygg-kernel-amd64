#pragma once
#include "sys/types.h"

#define IF_F_HASIP      (1 << 0)

#define IF_T_ETH        1

struct netdev;

typedef int (*netdev_send_func_t) (struct netdev *, const void *, size_t);

struct netdev {
    char name[32];
    uint8_t hwaddr[6];
    // Only one inaddr now
    uint32_t inaddr;
    uint32_t flags;
    int type;

    // TODO: arp table here?

    netdev_send_func_t send;

    // Physical device
    void *device;
    struct netdev *next;
};

struct netdev *netdev_create(int type);
