#pragma once
#include "sys/types.h"

struct netdev;
// Takes up a single page
#define PACKET_DATA_MAX         (4064)
struct packet {
    size_t size;
    struct packet *prev, *next;
    struct netdev *dev;
    char data[PACKET_DATA_MAX];
};
