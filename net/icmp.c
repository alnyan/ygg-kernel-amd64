#include "net/packet.h"
#include "sys/string.h"
#include "user/inet.h"
#include "sys/debug.h"
#include "net/util.h"
#include "net/inet.h"
#include "net/icmp.h"
#include "net/eth.h"
#include "net/if.h"

static void icmp_reply_echo(struct netdev *dev, uint32_t inaddr, struct icmp_frame *req, void *data, size_t len) {
    struct packet *p = packet_create(PACKET_SIZE_L3INET + sizeof(struct icmp_frame) + len);
    struct icmp_frame *icmp = PACKET_L4(p);
    icmp->type = ICMP_T_ECHOREPLY;
    icmp->code = 0;
    icmp->echo.id = req->echo.id;
    icmp->echo.seq = req->echo.seq;
    icmp->checksum = 0;

    memcpy(&icmp[1], data, len);

    icmp->checksum = inet_checksum(icmp, len + sizeof(struct icmp_frame));

    inet_send_wrapped(dev, inaddr, INET_P_ICMP, p);
}

void icmp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *data, size_t len) {
    if (len < sizeof(struct icmp_frame)) {
        kwarn("%s: dropping undersized frame\n", p->dev->name);
        return;
    }

    struct icmp_frame *icmp = data;

    len -= sizeof(struct icmp_frame);
    data += sizeof(struct icmp_frame);

    switch (icmp->type) {
    case ICMP_T_ECHO:
        icmp_reply_echo(p->dev, ntohl(ip->src_inaddr), icmp, data, len);
        break;
    default:
        kwarn("%s: dropping unknown type %02x\n", p->dev->name, icmp->type);
        break;
    }
}
