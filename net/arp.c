#include "net/packet.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "net/util.h"
#include "net/arp.h"
#include "net/eth.h"
#include "net/if.h"

struct arp_ent {
    uint32_t inaddr;
    uint8_t hwaddr[6];
    int resolved;
    struct arp_ent *prev, *next;
};

static struct arp_ent *arp_find_inaddr(struct netdev *dev, uint32_t inaddr) {
    for (struct arp_ent *ent = dev->arp_ent_head; ent; ent = ent->next) {
        if (ent->inaddr == inaddr) {
            return ent;
        }
    }
    return NULL;
}

static void arp_add_entry(struct netdev *dev, uint32_t inaddr, const uint8_t *hw) {
    struct arp_ent *ent = kmalloc(sizeof(struct arp_ent));
    _assert(ent);

    ent->inaddr = inaddr;
    memcpy(ent->hwaddr, hw, 6);
    ent->resolved = 1;

    ent->next = dev->arp_ent_head;
    ent->prev = NULL;
    dev->arp_ent_head = ent;

    char mac[24];
    strmac(mac, hw);
    kdebug("%s: add arp entry: " FMT_INADDR " -> %s\n", dev->name, VA_INADDR(inaddr), mac);
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
        kinfo("Found source addr\n");
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
