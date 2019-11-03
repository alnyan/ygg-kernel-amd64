#include "sys/net/netdev.h"
#include "sys/net/in.h"
#include "sys/heap.h"

void link_add_route_in(struct netdev *dev, uint32_t net, uint32_t mask, uint32_t gw) {
    struct in_route *route = (struct in_route *) kmalloc(sizeof(struct in_route));
    route->dev = dev;
    route->network_inaddr = net;
    route->mask = mask;
    route->gateway = gw;
    route->next = NULL;

    if (dev->link.in_routes_tail) {
        dev->link.in_routes_tail->next = route;
    } else {
        dev->link.in_routes_head = route;
    }
    dev->link.in_routes_tail = route;
}

struct in_route *link_find_route_in(struct netdev *dev, uint32_t dst_inaddr) {
    for (struct in_route *route = dev->link.in_routes_head; route; route = route->next) {
        if (route->network_inaddr == (dst_inaddr & route->mask)) {
            return route;
        }
    }

    return NULL;
}
