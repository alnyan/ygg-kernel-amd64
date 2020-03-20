#include "net/packet.h"
#include "sys/debug.h"
#include "net/util.h"
#include "net/arp.h"
#include "net/if.h"


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

    if (ntohs(arp->htype) != ARP_H_ETH) {
        kwarn("%s: dropping non-ethernet packet\n", p->dev->name);
        return;
    }
    if (ntohs(arp->ptype) != ARP_P_IP) {
        kwarn("%s: dropping non-IP packet\n", p->dev->name);
        return;
    }
}
