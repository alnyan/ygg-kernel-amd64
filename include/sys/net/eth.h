#pragma once
#include "sys/types.h"
#include "sys/net/netdev.h"

#define ETHERTYPE_ARP       0x0806
#define ETHERTYPE_IPV4      0x0800

struct eth_frame {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    char payload[];
} __attribute__((packed));

// Packet queue entry
struct ethq {
    struct ethq *next;
    struct netdev *dev;
    char data[];
};

// Ethernet queue stuff
void ethq_handle(void);

// TODO: replace (void *) with (struct netdev *)
void eth_handle_frame(struct netdev *dev, struct eth_frame *frame, size_t packet_size);
