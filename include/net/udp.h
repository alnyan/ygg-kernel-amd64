#pragma once
#include "sys/types.h"

struct udp_frame {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
};

struct eth_frame;
struct inet_frame;
struct packet;

void udp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *data, size_t len);
