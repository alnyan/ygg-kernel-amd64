#include "sys/net/in.h"
#include "sys/net/net.h"
#include "sys/net/arp.h"
#include "sys/assert.h"
#include "sys/debug.h"

struct udp_frame {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
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

void in_handle_frame(struct netdev *dev, struct in_frame *frame) {
    kdebug("INET frame\n");
    kdebug("\tTotal length: %u\n", htons(frame->length));
    kdebug("\tsrcaddr: " INADDR_FMT "\n", INADDR_VA(frame->src_inaddr));
    kdebug("\tdstaddr: " INADDR_FMT "\n", INADDR_VA(frame->dst_inaddr));

    uint16_t checksum = 0;

    for (size_t i = 0; i < sizeof(struct in_frame) / 2; ++i) {
        checksum += ((uint16_t *) frame)[i];
    }
    kdebug("IP checksum = %04x\n", checksum);

    //if (checksum != 0xFFFF) {
    //    kwarn("Packet IP checksum mismatch\n");
    //    // Drop packet
    //    return;
    //}

    switch (frame->protocol) {
    case IN_PROTO_UDP:
        udp_handle_frame(dev, 0, frame, (struct udp_frame *) frame->payload);
        break;
    default:
        kwarn("Unhandled IP protocol: %02x\n", frame->protocol);
        break;
    }
}
