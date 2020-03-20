#pragma once
#include "sys/types.h"

/* Protocol families */
#define PF_INET         2
#define AF_INET         PF_INET

/* Socket types */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2

#define SA_MAX_SIZE     64
struct sockaddr {
    uint16_t sa_family;
    char __pad[SA_MAX_SIZE - sizeof(uint16_t)];
} __attribute__((packed));
