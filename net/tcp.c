#include "net/packet.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "net/inet.h"
#include "net/util.h"
#include "net/eth.h"
#include "net/tcp.h"
#include "net/if.h"

static uint16_t tcp_checksum(uint32_t daddr, uint32_t saddr, uint16_t tcp_length, void *tcp_data) {
    uint32_t sum = 0;
    uint32_t tmp = saddr;
    sum += tmp & 0xFFFF;
    sum += tmp >> 16;
    tmp = daddr;
    sum += tmp & 0xFFFF;
    sum += tmp >> 16;
    sum += 6;
    sum += tcp_length;

    for (int i = 0; i < tcp_length / 2; ++i) {
        sum += ntohs(((uint16_t *) tcp_data)[i]);
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    return ~(sum & 0xFFFF) & 0xFFFF;
}

// Reset a connection attempt given its SYN packet
static void tcp_syn_reset(struct netdev *dev, uint32_t inaddr, struct tcp_frame *syn_frame) {
    if (!(dev->flags & IF_F_HASIP)) {
        return;
    }

    char packet[sizeof(struct eth_frame) + sizeof(struct inet_frame) + sizeof(struct tcp_frame)];
    struct tcp_frame *tcp = (struct tcp_frame *) &packet[sizeof(struct eth_frame) + sizeof(struct inet_frame)];
    memset(tcp, 0, sizeof(struct tcp_frame));

    tcp->src_port = syn_frame->dst_port;
    tcp->dst_port = syn_frame->src_port;
    tcp->ack_seq = htonl(ntohl(syn_frame->seq) + 1);
    tcp->doff = sizeof(struct tcp_frame) / 4;
    tcp->rst = 1;
    tcp->ack = 1;
    tcp->window = syn_frame->window;

    uint16_t checksum = tcp_checksum(inaddr, dev->inaddr, sizeof(struct tcp_frame), tcp);
    tcp->checksum = htons(checksum);

    debug_dump(DEBUG_DEFAULT, tcp, sizeof(struct tcp_frame));

    inet_send_wrapped(dev, inaddr, INET_P_TCP, packet, sizeof(packet));
}

void tcp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *data, size_t len) {
    if (len < sizeof(struct tcp_frame)) {
        kwarn("%s: dropping undersized frame\n", p->dev->name);
        return;
    }

    struct tcp_frame *tcp = data;
    size_t frame_size = 4 * tcp->doff;

    if (len < frame_size) {
        kwarn("%s: dropping undersized frame\n", p->dev->name);
        return;
    }

    data += frame_size;
    len -= frame_size;

    if (tcp->syn && !tcp->ack) {
        // TODO: Check if it's an initial SYN
        // Just reset the connection for now
        tcp_syn_reset(p->dev, ntohl(ip->src_inaddr), tcp);
    } else {
        kwarn("%s: unhandled packet: syn=%d, ack=%d, psh=%d, rst=%d\n", tcp->syn, tcp->ack, tcp->psh, tcp->rst);
    }
}
