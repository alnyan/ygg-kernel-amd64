#pragma once
#include "sys/types.h"

// Takes up a single page
#define PACKET_DATA_MAX         (4064)
struct packet {
    size_t size;
    struct packet *prev, *next;
    void *source_if;
    char data[PACKET_DATA_MAX];
};
