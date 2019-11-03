#pragma once
#include "sys/net/netdev.h"

#define INADDR_FMT          "%u.%u.%u.%u"
#define INADDR_VA(x)        ((x) & 0xFF), (((x) >> 8) & 0xFF), \
                            (((x) >> 16) & 0xFF), (((x) >> 24) & 0xFF)

#define IN_PROTO_UDP        0x11

struct in_frame {
    uint8_t version;
    uint8_t dscp;
    uint16_t length;
    uint16_t id;
    uint16_t flag_frag_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_chksum;
    uint32_t src_inaddr;
    uint32_t dst_inaddr;
    char payload[];
} __attribute__((packed));

struct in_route {
    struct netdev *dev;
    uint32_t mask;
    uint32_t network_inaddr;
    uint32_t gateway;
    struct in_route *next;
};

// frame is required to be a fully-wrapped L2/L3/payload frame for now
void in_send(struct netdev *dev, char *frame, size_t length);
void in_handle_frame(struct netdev *dev, struct in_frame *frame);
