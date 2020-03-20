#include "net/packet.h"
#include "sys/debug.h"
#include "net/util.h"
#include "net/eth.h"

#include "net/arp.h"

void eth_handle_frame(struct packet *p) {
    struct eth_frame *eth = (struct eth_frame *) p->data;

    if (p->size < sizeof(struct eth_frame)) {
        kwarn("Dropping undersized packet: %u\n", p->size);
        return;
    }

    switch (ntohs(eth->ethertype)) {
    case ETH_T_ARP:
        arp_handle_frame(p, p->data + sizeof(struct eth_frame), p->size - sizeof(struct eth_frame));
        break;
    case ETH_T_IP:
    case ETH_T_IPv6:
        // Silently drop
        break;
    default:
        kwarn("Dropping unknown packet: %04x\n", ntohs(eth->ethertype));
        break;
    }
}
