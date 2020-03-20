#pragma once
#include "sys/types.h"

#define ETH_T_IP            0x0800
#define ETH_T_IPv6          0x86DD
#define ETH_T_ARP           0x0806

struct eth_frame {
    uint8_t dst_hwaddr[6];
    uint8_t src_hwaddr[6];
    uint16_t ethertype;
} __attribute__((packed));

struct netdev;
struct packet;
void eth_handle_frame(struct packet *p);

int eth_send_wrapped(struct netdev *src, const uint8_t *dst, uint16_t et, void *data, size_t len);
