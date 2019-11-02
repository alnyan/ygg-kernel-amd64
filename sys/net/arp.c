#include "sys/net/arp.h"
#include "sys/net/eth.h"
#include "sys/net/net.h"
#include "sys/net/in.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/assert.h"
#include "sys/heap.h"

static struct arp_route *arp_routes_head = NULL;
static struct arp_route *arp_routes_tail = NULL;

void arp_add_route_in(struct netdev *dev, uint32_t inaddr, uint8_t *hwaddr) {
    struct arp_route *rt = (struct arp_route *) kmalloc(sizeof(struct arp_route));
    _assert(rt);
    kdebug("Added ARP route: " INADDR_FMT " is " MAC_FMT "\n", INADDR_VA(inaddr), MAC_VA(hwaddr));

    memcpy(rt->hwaddr, hwaddr, 6);
    rt->dev = dev;
    rt->addr_in = inaddr;
    rt->next = NULL;

    if (arp_routes_tail) {
        arp_routes_tail->next = rt;
    } else {
        arp_routes_head = rt;
    }
    arp_routes_tail = rt;
}

struct arp_route *arp_find_route_in(struct netdev *dev, uint32_t inaddr) {
    for (struct arp_route *rt = arp_routes_head; rt; rt = rt->next) {
        if (dev == rt->dev && rt->addr_in == inaddr) {
            return rt;
        }
    }
    return NULL;
}

void arp_handle_frame(struct netdev *dev, struct arp_frame *arp_frame) {
    uint16_t oper = htons(arp_frame->oper);

    kdebug("ARP frame:\n");
    kdebug("\tOperation: %s\n", (oper == 1) ? "Request" : "Reply");
    kdebug("\tSHA: " MAC_FMT "\n", MAC_VA(arp_frame->sha));
    kdebug("\tTHA: " MAC_FMT "\n", MAC_VA(arp_frame->tha));
    kdebug("\tSPA: " INADDR_FMT "\n", INADDR_VA(arp_frame->spa));
    kdebug("\tTPA: " INADDR_FMT "\n", INADDR_VA(arp_frame->tpa));

    struct arp_route *rt;
    if ((rt = arp_find_route_in(dev, arp_frame->spa))) {
        memcpy(rt->hwaddr, arp_frame->sha, 6);
    }

    // TODO: check that TPA actually belongs to a netdev
    if (arp_frame->tpa == 0x0F02000A /* 10.0.2.15 */) {
        if (!rt) {
            arp_add_route_in(dev, arp_frame->spa, arp_frame->sha);
        }

        if (oper == 1) {
            // Someone requested a MAC for IPv4
            char packet[sizeof(struct eth_frame) + sizeof(struct arp_frame)] = {0};
            struct eth_frame *r_eth_frame = (struct eth_frame *) packet;
            struct arp_frame *r_arp_frame = (struct arp_frame *) r_eth_frame->payload;

            // ETH
            r_eth_frame->ethertype = htons(ETHERTYPE_ARP);
            memcpy(r_eth_frame->dst_mac, arp_frame->sha, 6);
            memcpy(r_eth_frame->src_mac, dev->hwaddr, 6);

            // ARP
            r_arp_frame->htype = arp_frame->htype;
            r_arp_frame->ptype = arp_frame->ptype;
            r_arp_frame->hlen = arp_frame->hlen;
            r_arp_frame->plen = arp_frame->plen;
            r_arp_frame->oper = htons(2);
            memcpy(r_arp_frame->sha, dev->hwaddr, 6);
            r_arp_frame->spa = arp_frame->tpa;
            memcpy(r_arp_frame->tha, arp_frame->sha, 6);
            r_arp_frame->tpa = arp_frame->spa;

            if (dev->tx) {
                dev->tx(dev, packet, sizeof(struct eth_frame) + sizeof(struct arp_frame));
            }
        }
    }
}
