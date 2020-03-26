#include "net/packet.h"
#include "user/errno.h"
#include "sys/string.h"
#include "user/inet.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "net/util.h"
#include "net/inet.h"
#include "net/icmp.h"
#include "net/arp.h"
#include "net/eth.h"
#include "net/udp.h"
#include "net/tcp.h"
#include "net/if.h"

uint16_t inet_checksum(const void *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len / 2; ++i) {
        sum += ((const uint16_t *) data)[i];
    }
    if (len % 2) {
        sum += ((const uint8_t *) data)[len - 1];
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    return ~(sum & 0xFFFF) & 0xFFFF;
}

int inet_send_wrapped(struct netdev *src, uint32_t inaddr, uint8_t proto, struct packet *p) {
    uint32_t src_inaddr;

    if (inaddr == INADDR_BROADCAST) {
        src_inaddr = 0;
    } else {
        if (!(src->flags & IF_F_HASIP)) {
            kwarn("%s: has no inaddr\n", src->name);
            return -EINVAL;
        }

        src_inaddr = src->inaddr;
    }

    struct inet_frame *ip = PACKET_L3(p);
    ip->dst_inaddr = htonl(inaddr);
    ip->src_inaddr = htonl(src_inaddr);
    ip->checksum = 0;
    ip->tos = 0;
    ip->ttl = 64;
    ip->id = 0;
    ip->ihl = sizeof(struct inet_frame) / 4;
    ip->version = 4;
    ip->tot_len = htons(p->size - sizeof(struct eth_frame));
    ip->flags = 0;
    ip->proto = proto;

    ip->checksum = inet_checksum(ip, sizeof(struct inet_frame));

    if (inaddr == INADDR_BROADCAST) {
        // No need for ARP when broadcasting
        return eth_send_wrapped(src, ETH_A_BROADCAST, ETH_T_IP, p);
    } else {
        return arp_send(src, inaddr, ETH_T_IP, p);
    }
}

int inet_send(uint32_t inaddr, uint8_t proto, struct packet *p) {
    // TODO: find out what to do when broadcasting here
    struct netdev *dev = netdev_find_inaddr(inaddr);
    if (!dev) {
        // TODO; ???
        kinfo("ENOENT\n");
        return -ENOENT;
    }

    return inet_send_wrapped(dev, inaddr, proto, p);
}

void inet_handle_frame(struct packet *p, struct eth_frame *eth, void *data, size_t len) {
    if (len < sizeof(struct inet_frame)) {
        kwarn("%s: dropping undersized frame\n", p->dev->name);
        return;
    }

    struct inet_frame *ip = data;

    if (ip->version != 4) {
        kwarn("%s: version != 4\n", p->dev->name);
        return;
    }

    size_t frame_size = ip->ihl * 4;
    if (len < frame_size) {
        kwarn("%s: dropping undersized frame\n", p->dev->name);
    }

    // Pick smaller of provided and NIC-received length
    // (RTL8139 pads packets with zeros for some reason)
    len = MIN(len, ntohs(ip->tot_len));

    len -= frame_size;
    data += frame_size;

    switch (ip->proto) {
    case INET_P_ICMP:
        icmp_handle_frame(p, eth, ip, data, len);
        break;
    case INET_P_UDP:
        udp_handle_frame(p, eth, ip, data, len);
        break;
    case INET_P_TCP:
        tcp_handle_frame(p, eth, ip, data, len);
        break;
    default:
        kwarn("%s: dropping unknown protocol %02x\n", p->dev->name, ip->proto);
        break;
    }
}
