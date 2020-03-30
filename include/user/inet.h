#pragma once
#include "sys/types.h"

#define INADDR_ANY          0
#define INADDR_BROADCAST    0xFFFFFFFF

#define INET_P_ICMP         1
#define INET_P_TCP          6
#define INET_P_UDP          17

#define IPPROTO_ICMP        (INET_P_ICMP)
#define IPPROTO_UDP         (INET_P_UDP)

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    // Differs from Linux sockets
    uint32_t sin_addr;
} __attribute__((packed));
