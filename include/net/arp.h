#pragma once
#include "sys/types.h"

#define ARP_H_ETH           1
#define ARP_P_IP            0x0800

#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

struct arp_frame {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    // INET only
    uint8_t sha[6];
    uint32_t spa;
    uint8_t tha[6];
    uint32_t tpa;
} __attribute__((packed));

struct packet;
void arp_handle_frame(struct packet *packet, void *data, size_t len);

int arp_send(struct netdev *dev, uint32_t inaddr, uint16_t etype, struct packet *p);
const uint8_t *arp_resolve(struct netdev *dev, uint32_t inaddr);
