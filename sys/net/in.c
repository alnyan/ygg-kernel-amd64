#include "sys/net/in.h"
#include "sys/net/net.h"
#include "sys/net/eth.h"
#include "sys/net/arp.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"

struct udp_frame {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
    char payload[];
};

struct icmp_frame {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t rest;
    char payload[];
};

static void udp_handle_frame(struct netdev *dev, int in6, void *frame, struct udp_frame *udp) {
    // in6 is required to be 0 now, I haven't yet implemented IPv6 support
    _assert(!in6);
    struct in_frame *in_frame = frame;

    // Ignore UDP checksums
    kdebug("UDP packet\n");
    kdebug("\tsrcport: %u\n", htons(udp->src_port));
    kdebug("\tdstport: %u\n", htons(udp->dst_port));
    kdebug("\tLength: %u\n", htons(udp->length));

    debug_dump(DEBUG_DEFAULT, udp->payload, htons(udp->length) - 8);
}

uint16_t in_checksum_compute(uint8_t *data, size_t length) {
    // Initialise the accumulator.
    uint32_t acc = 0xffff;

    // Handle complete 16-bit blocks.
    for (size_t i = 0; i + 1 < length; i += 2) {
        uint16_t word;

        memcpy(&word, data + i, 2);
        acc += htons(word);
        if (acc > 0xffff) {
            acc -= 0xffff;
        }
    }

    // Handle any partial block at the end of the data.
    if (length & 1) {
        uint16_t word = 0;
        memcpy(&word, data + length - 1, 1);
        acc += htons(word);
        if (acc > 0xffff) {
            acc -= 0xffff;
        }
    }

    // Return the checksum in network byte order.
    return htons(~acc);
}

int in_send_frame(struct netdev *dev, char *frame, size_t size) {
    /*
     * Caller is required to fill the following:
     *  - in_frame: everything except TTL (optional) and src_inaddr
     */
    struct eth_frame *eth_frame = (struct eth_frame *) frame;
    struct in_frame *in_frame = (struct in_frame *) eth_frame->payload;
    char *payload = in_frame->payload;

    kdebug("Send in_frame to " INADDR_FMT "\n", INADDR_VA(in_frame->dst_inaddr));

    struct in_route *route = link_find_route_in(dev, in_frame->dst_inaddr);
    if (!route) {
        kwarn(INADDR_FMT ": no route to host\n", INADDR_VA(in_frame->dst_inaddr));
        return -1;
    }
    kdebug("Found a route to " INADDR_FMT " via " INADDR_FMT "\n",
            INADDR_VA(in_frame->dst_inaddr),
            INADDR_VA(route->gateway));


    // Find MAC route to next hop
    struct arp_route *arp_route = arp_find_route_in(dev, route->gateway);
    if (!arp_route) {
        kwarn(INADDR_FMT ": no route to gateway\n", INADDR_VA(route->gateway));
        return -1;
    }

    // L3:
    // TODO: get actual address from netdev->link_info
    in_frame->src_inaddr = 0x0F02000A;
    in_frame->version = 0x45;
    in_frame->dscp = 0;
    in_frame->length = htons(size - sizeof(struct eth_frame));
    in_frame->ttl = 0x40;
    in_frame->header_chksum = 0;
    in_frame->header_chksum = in_checksum_compute((uint8_t *) in_frame, htons(in_frame->length));

    // L2:
    // Fill netdev->hwaddr -> eth_frame->src_mac
    memcpy(eth_frame->src_mac, dev->hwaddr, 6);
    memcpy(eth_frame->dst_mac, arp_route->hwaddr, 6);
    eth_frame->ethertype = htons(ETHERTYPE_IPV4);

    debug_dump(DEBUG_DEFAULT, frame, size);

    dev->tx(dev, frame, size);

    return 0;
}

static void icmp_handle_frame(struct netdev *dev, int in6, void *frame, struct icmp_frame *icmp) {
    // in6 is required to be 0 now, I haven't yet implemented IPv6 support
    _assert(!in6);
    struct in_frame *in_frame = frame;

    if (icmp->type == 0x08) {
        size_t payload_length = htons(in_frame->length) - sizeof(struct in_frame) - 8;
        size_t reply_size = htons(in_frame->length) + sizeof(struct eth_frame);

        char reply[reply_size];
        struct eth_frame *eth_reply_frame = (struct eth_frame *) reply;
        struct in_frame *in_reply_frame = (struct in_frame *) eth_reply_frame->payload;
        struct icmp_frame *icmp_reply_frame = (struct icmp_frame *) in_reply_frame->payload;

        // L4
        memcpy(icmp_reply_frame, icmp, payload_length + 8);
        icmp_reply_frame->type = 0;
	icmp_reply_frame->checksum = 0;
	icmp_reply_frame->checksum = in_checksum_compute((uint8_t *) icmp_reply_frame, payload_length + 8);

        // L3
        in_reply_frame->dst_inaddr = in_frame->src_inaddr;
        in_reply_frame->id = in_frame->id;
        in_reply_frame->flag_frag_offset = in_frame->flag_frag_offset;
        in_reply_frame->protocol = in_frame->protocol;

        kdebug("PING REPLY\n");

        in_send_frame(dev, reply, reply_size);
    }
}

void in_handle_frame(struct netdev *dev, struct in_frame *frame) {
    kdebug("INET frame\n");
    kdebug("\tTotal length: %u\n", htons(frame->length));
    kdebug("\tsrcaddr: " INADDR_FMT "\n", INADDR_VA(frame->src_inaddr));
    kdebug("\tdstaddr: " INADDR_FMT "\n", INADDR_VA(frame->dst_inaddr));

    uint16_t checksum = in_checksum_compute((uint8_t *) frame, htons(frame->length));
    kdebug("IP checksum = %04x\n", checksum);

    if (checksum != 0) {
        kwarn("Packet IP checksum mismatch\n");
        // Drop packet
        return;
    }

    switch (frame->protocol) {
    case 0x01:
        icmp_handle_frame(dev, 0, frame, (struct icmp_frame *) frame->payload);
        break;
    case IN_PROTO_UDP:
        udp_handle_frame(dev, 0, frame, (struct udp_frame *) frame->payload);
        break;
    default:
        kwarn("Unhandled IP protocol: %02x\n", frame->protocol);
        break;
    }
}
