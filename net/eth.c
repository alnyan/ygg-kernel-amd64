#include "net/packet.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "net/util.h"
#include "net/eth.h"
#include "net/if.h"

#include "net/inet.h"
#include "net/arp.h"

int eth_send_wrapped(struct netdev *src, const uint8_t *hwaddr, uint16_t et, struct packet *p) {
    struct eth_frame *eth = PACKET_L2(p);

    memcpy(eth->dst_hwaddr, hwaddr, 6);
    memcpy(eth->src_hwaddr, src->hwaddr, 6);
    eth->ethertype = htons(et);

    _assert(src->send);
    return src->send(src, p);
}

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
        inet_handle_frame(p, eth, p->data + sizeof(struct eth_frame), p->size - sizeof(struct eth_frame));
        break;
    case ETH_T_IPv6:
        // Silently drop
        break;
    default:
        kwarn("%s: dropping unknown type: %04x\n", p->dev->name, ntohs(eth->ethertype));
        break;
    }
}
