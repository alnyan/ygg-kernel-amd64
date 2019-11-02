#include "sys/net/eth.h"
#include "sys/net/net.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/spin.h"

#include "sys/net/arp.h"
#include "sys/net/in.h"

static spin_t ethq_lock = 0;
static int ethq_count = 0;
static struct ethq *ethq_head = NULL, *ethq_tail = NULL;

static struct ethq *ethq_pop(void) {
    if (ethq_head) {
        struct ethq *head = ethq_head;
        if (!head->next) {
            ethq_tail = NULL;
        }
        ethq_head = head->next;
        return head;
    }
    return NULL;
}

static void ethq_add(struct netdev *dev, struct eth_frame *eth_frame, size_t packet_size) {
    struct ethq *ent = (struct ethq *) kmalloc(packet_size + sizeof(struct ethq));
    ent->dev = dev;
    ent->next = NULL;
    memcpy(ent->data, eth_frame, packet_size);

    spin_lock(&ethq_lock);
    if (ethq_tail) {
        ethq_tail->next = ent;
    } else {
        ethq_head = ent;
    }
    ethq_tail = ent;
    ++ethq_count;
    spin_release(&ethq_lock);

    kdebug("Added\n");
}

// The function takes a single packet from the queue
// and processes it
void ethq_handle(void) {
    spin_lock(&ethq_lock);
    struct ethq *ethq = ethq_pop();
    spin_release(&ethq_lock);

    if (ethq) {
        struct eth_frame *eth_frame = (struct eth_frame *) ethq->data;

        switch (eth_frame->ethertype) {
        case htons(ETHERTYPE_ARP):
            arp_handle_frame(ethq->dev, (struct arp_frame *) eth_frame->payload);
            break;
        case htons(ETHERTYPE_IPV4):
            in_handle_frame(ethq->dev, (struct in_frame *) eth_frame->payload);
            break;
        default:
            kdebug("Unsupported frame type: %04x\n", htons(eth_frame->ethertype));
            break;
        }

        kfree(ethq);
    }
}

// Called from interrupt handler
void eth_handle_frame(struct netdev *dev, struct eth_frame *eth_frame, size_t packet_size) {
    debug_dump(DEBUG_DEFAULT, eth_frame, packet_size);

    if (htons(eth_frame->ethertype) >= 1536) {
        ethq_add(dev, eth_frame, packet_size);
    }
    // Ignore all other packets
}
