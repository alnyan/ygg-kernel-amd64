#pragma once
#include "sys/types.h"

#define IF_F_HASIP      (1 << 0)

#define IF_T_ETH        1

struct netdev;
struct arp_ent;
struct packet;

typedef int (*netdev_send_func_t) (struct netdev *, struct packet *);

struct netdev {
    char name[32];
    uint8_t hwaddr[6];
    // Only one inaddr now
    uint32_t inaddr;
    uint32_t flags;
    int type;

    // ARP entry list
    struct arp_ent *arp_ent_head;

    netdev_send_func_t send;

    // Physical device
    void *device;
    struct netdev *next;
};

struct netdev *netdev_create(int type);
struct netdev *netdev_by_name(const char *name);
struct netdev *netdev_find_inaddr(uint32_t inaddr);

int netctl(struct netdev *dev, uint32_t op, void *arg);
