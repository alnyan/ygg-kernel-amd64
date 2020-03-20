#pragma once
#include "sys/types.h"

#define INADDR_ANY          0
#define INADDR_BROADCAST    0xFFFFFFFF


struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    // Differs from Linux sockets
    uint32_t sin_addr;
} __attribute__((packed));
