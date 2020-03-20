#pragma once
#include "sys/types.h"

#define ICMP_T_ECHO         8
#define ICMP_T_ECHOREPLY    0

struct icmp_frame {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    union {
        struct {
            uint16_t id;
            uint16_t seq;
        } echo;
    };
};

struct packet;
struct eth_frame;
struct inet_frame;
void icmp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *d, size_t len);
