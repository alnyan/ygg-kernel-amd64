#pragma once
#include "sys/types.h"

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    // Differs from Linux sockets
    uint32_t sin_addr;
} __attribute__((packed));
