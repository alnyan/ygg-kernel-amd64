#pragma once
#include "sys/net/netdev.h"

struct arp_frame {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint32_t spa;
    uint8_t tha[6];
    uint32_t tpa;
} __attribute__((packed));

struct arp_route {
    struct netdev *dev;
    uint32_t addr_in;
    uint8_t hwaddr[6];
    struct arp_route *next;
};

struct arp_route *arp_find_route_in(struct netdev *dev, uint32_t inaddr);
void arp_handle_frame(struct netdev *dev, struct arp_frame *arp_frame);
