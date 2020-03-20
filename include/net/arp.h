#pragma once
#include "sys/types.h"

#define ARP_H_ETH           1
#define ARP_P_IP            0x0800

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
