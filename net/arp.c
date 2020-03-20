#include "arch/amd64/mm/phys.h"
#include "net/packet.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "net/util.h"
#include "net/arp.h"
#include "net/eth.h"
#include "net/if.h"
#include "sys/mm.h"

struct arp_hold;

struct arp_ent {
    uint32_t inaddr;
    uint8_t hwaddr[6];
    int resolved;
    struct arp_hold *hold;
    struct arp_ent *prev, *next;
};

struct arp_hold {
    uint16_t ethertype;
    size_t len;
    struct arp_hold *next;
    char data[4096 - sizeof(uint16_t) - sizeof(struct arp_hold *) - sizeof(size_t)];
} __attribute__((packed));

static void arp_send_request(struct netdev *dev, uint32_t inaddr);

static struct arp_ent *arp_find_inaddr(struct netdev *dev, uint32_t inaddr) {
    for (struct arp_ent *ent = dev->arp_ent_head; ent; ent = ent->next) {
        if (ent->inaddr == inaddr) {
            return ent;
        }
    }
    return NULL;
}

static struct arp_ent *arp_add_entry(struct netdev *dev, uint32_t inaddr, const uint8_t *hw) {
    struct arp_ent *ent = kmalloc(sizeof(struct arp_ent));
    _assert(ent);

    ent->inaddr = inaddr;
    if (hw) {
        char mac[24];
        strmac(mac, hw);
        kdebug("%s: add arp entry: " FMT_INADDR " -> %s\n", dev->name, VA_INADDR(inaddr), mac);

        memcpy(ent->hwaddr, hw, 6);
        ent->resolved = 1;
    } else {
        ent->resolved = 0;
        kdebug("%s: pending resolution: " FMT_INADDR "\n", dev->name, VA_INADDR(inaddr));
    }

    ent->hold = NULL;
    ent->next = dev->arp_ent_head;
    ent->prev = NULL;
    dev->arp_ent_head = ent;

    return ent;
}

static struct arp_ent *arp_find_or_create(struct netdev *dev, uint32_t inaddr) {
    struct arp_ent *ent;
    if ((ent = arp_find_inaddr(dev, inaddr)) != NULL) {
        return ent;
    }

    // Send a request for inaddr
    arp_send_request(dev, inaddr);

    return arp_add_entry(dev, inaddr, NULL);
}

static void arp_send_reply(struct netdev *src, struct arp_frame *req) {
    char reply[sizeof(struct eth_frame) + sizeof(struct arp_frame)];
    struct arp_frame *arp = (struct arp_frame *) &reply[sizeof(struct eth_frame)];

    memcpy(arp->tha, req->sha, 6);
    memcpy(arp->sha, src->hwaddr, 6);

    arp->spa = req->tpa;
    arp->tpa = req->spa;

    arp->hlen = req->hlen;
    arp->plen = req->plen;
    arp->htype = req->htype;
    arp->ptype = req->ptype;

    arp->oper = htons(ARP_OP_REPLY);

    eth_send_wrapped(src, arp->tha, ETH_T_ARP, reply, sizeof(reply));
}

int arp_send(struct netdev *src, uint32_t inaddr, uint16_t ethertype, void *data, size_t len) {
    struct arp_ent *ent = arp_find_or_create(src, inaddr);
    _assert(ent);

    if (ent->resolved) {
        return eth_send_wrapped(src, ent->hwaddr, ethertype, data, len);
    } else {
        // TODO: better way?
        uintptr_t page = amd64_phys_alloc_page();
        _assert(page != MM_NADDR);
        struct arp_hold *hold = (struct arp_hold *) MM_VIRTUALIZE(page);

        hold->ethertype = ethertype;
        hold->len = len;
        memcpy(hold->data, data, len);

        hold->next = ent->hold;
        ent->hold = hold;

        return 0;
    }
}

static void arp_send_request(struct netdev *dev, uint32_t inaddr) {
    _assert(dev->flags & IF_F_HASIP);
    char packet[sizeof(struct eth_frame) + sizeof(struct arp_frame)];
    struct arp_frame *arp = (struct arp_frame *) &packet[sizeof(struct eth_frame)];

    memcpy(arp->sha, dev->hwaddr, 6);
    memset(arp->tha, 0, 6);

    arp->spa = htonl(dev->inaddr);
    arp->tpa = htonl(inaddr);

    arp->hlen = 6;
    arp->plen = 4;
    arp->htype = htons(ARP_H_ETH);
    arp->ptype = htons(ARP_P_IP);

    arp->oper = htons(ARP_OP_REQUEST);

    kinfo("Send request for " FMT_INADDR "\n", VA_INADDR(inaddr));
    eth_send_wrapped(dev, ETH_A_BROADCAST, ETH_T_ARP, packet, sizeof(packet));
}

/*
?Do I have the hardware type in ar$hrd?
Yes: (almost definitely)
  [optionally check the hardware length ar$hln]
  ?Do I speak the protocol in ar$pro?
  Yes:
    [optionally check the protocol length ar$pln]
    Merge_flag := false
    If the pair <protocol type, sender protocol address> is
        already in my translation table, update the sender
        hardware address field of the entry with the new
        information in the packet and set Merge_flag to true.
    ?Am I the target protocol address?
    Yes:
      If Merge_flag is false, add the triplet <protocol type,
          sender protocol address, sender hardware address> to
          the translation table.
      ?Is the opcode ares_op$REQUEST?  (NOW look at the opcode!!)
      Yes:
        Swap hardware and protocol fields, putting the local
            hardware and protocol addresses in the sender fields.
        Set the ar$op field to ares_op$REPLY
        Send the packet to the (new) target hardware address on
            the same hardware on which the request was received.
*/

void arp_handle_frame(struct packet *p, void *data, size_t len) {
    if (len < sizeof(struct arp_frame)) {
        kwarn("%s: dropping undersized frame: %u\n", p->dev->name, len);
        return;
    }

    struct arp_frame *arp = data;
    struct arp_ent *ent;
    struct netdev *dev = p->dev;
    int merge = 0;

    if (ntohs(arp->htype) != ARP_H_ETH) {
        kwarn("%s: dropping non-ethernet packet\n", p->dev->name);
        return;
    }
    if (ntohs(arp->ptype) != ARP_P_IP) {
        kwarn("%s: dropping non-IP packet\n", p->dev->name);
        return;
    }

    if ((ent = arp_find_inaddr(dev, ntohl(arp->spa))) != NULL) {
        merge = 1;

        memcpy(ent->hwaddr, arp->sha, 6);

        if (!ent->resolved) {
            ent->resolved = 1;
            struct arp_hold *hold;
            while ((hold = ent->hold)) {
                ent->hold = hold->next;

                eth_send_wrapped(dev, ent->hwaddr, hold->ethertype, hold->data, hold->len);

                amd64_phys_free(MM_PHYS(hold));
            }
        }
    }

    if ((dev->flags & IF_F_HASIP) &&
        dev->inaddr == ntohl(arp->tpa)) {
        if (!merge) {
            arp_add_entry(dev, ntohl(arp->spa), arp->sha);
        }

        if (htons(arp->oper) == ARP_OP_REQUEST) {
            arp_send_reply(dev, arp);
        }
    }
}
