#include "net/packet.h"
#include "sys/debug.h"
#include "net/util.h"
#include "net/eth.h"
#include "net/if.h"

#include "net/arp.h"

void eth_handle_frame(struct packet *p) {
    if (p->size < sizeof(struct eth_frame)) {
        kwarn("%s: dropping undersized packet: %u\n", p->dev->name, p->size);
        return;
    }

    struct eth_frame *eth = (struct eth_frame *) p->data;

    switch (ntohs(eth->ethertype)) {
    case ETH_T_ARP:
        arp_handle_frame(p, p->data + sizeof(struct eth_frame), p->size - sizeof(struct eth_frame));
        break;
    case ETH_T_IP:
    case ETH_T_IPv6:
        // Silently drop
        break;
    default:
        kwarn("%s: dropping unknown type: %04x\n", p->dev->name, ntohs(eth->ethertype));
        break;
    }
}
