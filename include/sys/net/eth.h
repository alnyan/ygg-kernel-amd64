#pragma once
#include "sys/types.h"

struct eth_frame {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    char payload[];
} __attribute__((packed));

// TODO: replace (void *) with (struct netdev *)
void eth_handle_frame(void *dev, struct eth_frame *frame, size_t packet_size);
