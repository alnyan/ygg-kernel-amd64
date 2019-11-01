#include "sys/net/eth.h"
#include "sys/net/net.h"
#include "sys/debug.h"

#define ETHERTYPE_ARP       0x0806
#define ETHERTYPE_IPV4      0x0800

#define INADDR_FMT          "%u.%u.%u.%u"
#define INADDR_VA(x)        ((x) & 0xFF), (((x) >> 8) & 0xFF), \
                            (((x) >> 16) & 0xFF), (((x) >> 24) & 0xFF)

struct arp_frame {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint32_t spa;
    uint8_t tha[6];
    uint32_t tpa;
} __attribute__((packed));

// Just for testing
static void arp_handle_frame(void *dev, struct arp_frame *arp_frame) {
    kdebug("ARP frame:\n");
    kdebug("\tOperation: %s\n", (htons(arp_frame->oper) == 1) ? "Request" : "Reply");
    kdebug("\tSHA: " MAC_FMT "\n", MAC_VA(arp_frame->sha));
    kdebug("\tTHA: " MAC_FMT "\n", MAC_VA(arp_frame->tha));
    kdebug("\tSPA: " INADDR_FMT "\n", INADDR_VA(arp_frame->spa));
    kdebug("\tTPA: " INADDR_FMT "\n", INADDR_VA(arp_frame->tpa));
}

void eth_handle_frame(void *dev, struct eth_frame *eth_frame, size_t packet_size) {
    debug_dump(DEBUG_DEFAULT, eth_frame, packet_size - 4);

    if (eth_frame->ethertype >= 1536) {
        kdebug("DST: " MAC_FMT ", SRC: " MAC_FMT "\n",
                MAC_VA(eth_frame->dst_mac),
                MAC_VA(eth_frame->src_mac));

        // Ethernet II frame
        switch (eth_frame->ethertype) {
        case htons(ETHERTYPE_ARP):
            arp_handle_frame(dev, (struct arp_frame *) eth_frame->payload);
            break;
            case htons(ETHERTYPE_IPV4):
            kdebug("IPv4 frame\n");
            break;
        }
    }
}
