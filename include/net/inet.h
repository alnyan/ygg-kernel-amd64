#pragma once
#include "sys/types.h"

#define INET_P_ICMP         1
#define INET_P_UDP          17

struct inet_frame {
    uint8_t ihl:4;
    uint8_t version:4;
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t flags;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint32_t src_inaddr;
    uint32_t dst_inaddr;
} __attribute__((packed));

struct netdev;
struct packet;
struct eth_frame;
void inet_handle_frame(struct packet *p, struct eth_frame *eth, void *data, size_t len);
int inet_send_wrapped(struct netdev *src, uint32_t inaddr, uint8_t proto, void *data, size_t len);

uint16_t inet_checksum(const void *data, size_t len);
