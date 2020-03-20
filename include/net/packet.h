#pragma once
#include "sys/types.h"

struct netdev;
// Takes up a single page
#define PACKET_DATA_MAX         (4056)
struct packet {
    size_t size;
    struct packet *prev, *next;
    struct netdev *dev;
    size_t refcount;
    char data[PACKET_DATA_MAX];
};

void packet_ref(struct packet *p);
void packet_unref(struct packet *p);
