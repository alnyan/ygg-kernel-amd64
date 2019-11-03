#pragma once
#include "sys/types.h"

struct netdev;

typedef int (*netdev_tx_func_t) (struct netdev *, const void *, size_t);

struct link_info {
    struct in_route *in_routes_head;
    struct in_route *in_routes_tail;
};

struct netdev {
    uint8_t hwaddr[6];
    char name[32];
    netdev_tx_func_t tx;

    struct link_info link;
};

void link_add_route_in(struct netdev *dev, uint32_t net, uint32_t mask, uint32_t gw);
struct in_route *link_find_route_in(struct netdev *dev, uint32_t dst_inaddr);
