#include "net/packet.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "net/inet.h"
#include "net/util.h"
#include "sys/attr.h"
#include "net/eth.h"
#include "net/tcp.h"
#include "net/if.h"

struct tcp_socket {
    enum {
        TCP_SOCK_NONE,      // Not connect()ed, not bind()ed
        TCP_SOCK_CLIENT,    // connect()ed
        TCP_SOCK_SERVER,    // bind()ed
    } type;
    enum {
        TCP_STA_INIT,           // All: initial state
        TCP_STA_SYNACK_SENT,    // Sent SYN+ACK, awaiting ACK
        TCP_STA_ESTABLISHED,    // Incoming:    received ACK from remote after sending SYN+ACK
                                // Outcoming:   received SYN+ACK from remote and sent ACK
    } state;
    // For all sockets
    uint16_t local_port;
    uint32_t local_seq;
    // For client sockets
    uint16_t remote_port;
    uint32_t remote_inaddr;
    uint32_t remote_seq;
};

static struct tcp_socket port9000 = {0};
static struct tcp_socket client = {0};

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

// Establish a remote -> local connection given its SYN packet
static void tcp_server_establish(struct netdev *dev, uint32_t inaddr, struct tcp_frame *syn, struct tcp_socket *serv) {
    client.local_seq = 32657;
    client.local_port = serv->local_port;
    client.remote_seq = ntohl(syn->seq);
    client.remote_port = ntohs(syn->src_port);
    client.remote_inaddr = inaddr;
    client.state = TCP_STA_SYNACK_SENT;
    client.type = TCP_SOCK_CLIENT;

    char packet[sizeof(struct eth_frame) + sizeof(struct inet_frame) + sizeof(struct tcp_frame)];
    struct tcp_frame *tcp = (struct tcp_frame *) &packet[sizeof(struct eth_frame) + sizeof(struct inet_frame)];
    memset(tcp, 0, sizeof(struct tcp_frame));

    tcp->src_port = ntohs(client.local_port);
    tcp->dst_port = ntohs(client.remote_port);
    tcp->seq = htonl(client.local_seq);
    tcp->ack_seq = htonl(++client.remote_seq);
    tcp->doff = sizeof(struct tcp_frame) / 4;
    tcp->ack = 1;
    tcp->syn = 1;
    tcp->window = syn->window;

    uint16_t checksum = tcp_checksum(inaddr, dev->inaddr, sizeof(struct tcp_frame), tcp);
    tcp->checksum = htons(checksum);

    kdebug("establish " FMT_INADDR ":%u -> :%u\n", VA_INADDR(inaddr), client.remote_port, client.local_port);

    inet_send_wrapped(dev, client.remote_inaddr, INET_P_TCP, packet, sizeof(packet));
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

    uint16_t port = ntohs(tcp->dst_port);
    if (tcp->syn && !tcp->ack) {
        if (port == 9000) {
            tcp_server_establish(p->dev, ntohl(ip->src_inaddr), tcp, &port9000);
        } else {
            tcp_syn_reset(p->dev, ntohl(ip->src_inaddr), tcp);
        }
    } else if (tcp->ack && !tcp->syn && !tcp->psh) {
        uint16_t remote_port = ntohs(tcp->src_port);
        uint32_t remote_inaddr = ntohl(ip->src_inaddr);

        if (port == 9000 && remote_port == client.remote_port && remote_inaddr == client.remote_inaddr) {
            if (client.state == TCP_STA_SYNACK_SENT) {
                kdebug("Remote side confirmed establishing\n");
                client.local_seq = 1;
                client.remote_seq = 1;
                client.state = TCP_STA_ESTABLISHED;
            } else {
                kwarn("Got ACK, but was not waiting for it\n");
            }
        }
    } else if (tcp->ack && tcp->psh) {
        uint16_t remote_port = ntohs(tcp->src_port);
        uint32_t remote_inaddr = ntohl(ip->src_inaddr);

        if (port == 9000 && remote_port == client.remote_port && remote_inaddr == client.remote_inaddr) {
            if (client.state == TCP_STA_ESTABLISHED) {
                kdebug("Got PSH+ACK on established socket\n");
                kdebug("Length = %u\n", len);
            } else {
                kwarn("Got PSH+ACK, but socket is not established\n");
            }
        }
    } else {
        kwarn("%s: unhandled packet: syn=%d, ack=%d, psh=%d, rst=%d\n", tcp->syn, tcp->ack, tcp->psh, tcp->rst);
    }
}

static __init void port9000_init(void) {
    port9000.local_port = 9000;
    port9000.local_seq = 1;
    port9000.state = TCP_STA_INIT;
    port9000.type = TCP_SOCK_SERVER;
}
