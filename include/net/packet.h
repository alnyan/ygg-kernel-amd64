#pragma once
#include "sys/types.h"
#include "sys/spin.h"

#define PACKET_SIZE_L3INET      (sizeof(struct eth_frame) + sizeof(struct inet_frame))
#define PACKET_SIZE_L3ARP       (sizeof(struct eth_frame) + sizeof(struct arp_frame))
#define PACKET_L2(p)            ((void *) ((p)->data))
// TODO: Only works with ethernet
#define PACKET_L3(p)            ((void *) ((p)->data + sizeof(struct eth_frame)))
// TODO: Only works for TCP/IP stack
#define PACKET_L4(p)            ((void *) ((p)->data + PACKET_SIZE_L3INET))

struct netdev;
// Takes up a single page
#define PACKET_DATA_MAX         (4056)
struct packet {
    size_t size;
    struct netdev *dev;
    size_t refcount;
    char data[PACKET_DATA_MAX];
};

struct packet_qh {
    struct packet *packet;
    struct packet_qh *prev, *next;
};

struct packet_queue {
    spin_t lock;
    struct packet_qh *head, *tail;
};

struct packet *packet_create(size_t size);
void packet_ref(struct packet *p);
void packet_unref(struct packet *p);

void packet_queue_init(struct packet_queue *q);
void packet_queue_push(struct packet_queue *q, struct packet *p);
struct packet *packet_queue_pop(struct packet_queue *q);
