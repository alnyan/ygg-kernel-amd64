#include "net/packet.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/panic.h"
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
static void tcp_packet_send(struct netdev *dev,
                            struct tcp_socket *sock,
                            struct tcp_flags flags,
                            const void *data,
                            size_t len) {
    if (!(dev->flags & IF_F_HASIP)) {
        return;
    }

    struct packet *p = packet_create(PACKET_SIZE_L3INET + sizeof(struct tcp_frame) + len);
    struct tcp_frame *tcp = PACKET_L4(p);
    void *dst_data = &tcp[1];
    memset(tcp, 0, sizeof(struct tcp_frame));
    memcpy(dst_data, data, len);

    tcp->src_port = htons(sock->local_port);
    tcp->dst_port = htons(sock->remote_port);
    tcp->seq = htonl(sock->local_seq);
    tcp->ack_seq = htonl(sock->remote_seq);
    tcp->doff = sizeof(struct tcp_frame) / 4;
    tcp->flags = flags;
    tcp->window = htons(64400);

    uint16_t checksum = tcp_checksum(sock->remote_inaddr, dev->inaddr, sizeof(struct tcp_frame), tcp);
    tcp->checksum = htons(checksum);

    inet_send_wrapped(dev, sock->remote_inaddr, INET_P_TCP, p);
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
    if (tcp->flags.syn && !tcp->flags.ack) {
        if (port == 9000) {
            // Establish local->remote connection
            client.local_seq = 32657;
            client.local_port = port9000.local_port;
            client.remote_seq = ntohl(tcp->seq);
            client.remote_port = ntohs(tcp->src_port);
            client.remote_inaddr = ntohl(ip->src_inaddr);
            client.state = TCP_STA_SYNACK_SENT;
            client.type = TCP_SOCK_CLIENT;

            // Reply with SYN+ACK
            struct tcp_flags flags = {
                .syn = 1,
                .ack = 1
            };
            ++client.remote_seq;
            tcp_packet_send(p->dev, &client, flags, NULL, 0);
        } else {
            // Reply with ACK+RST
            //    tcp_syn_reset(p->dev, ntohl(ip->src_inaddr), tcp);
        }
    } else if (tcp->flags.ack && !tcp->flags.syn) {
        uint16_t remote_port = ntohs(tcp->src_port);
        uint32_t remote_inaddr = ntohl(ip->src_inaddr);
        uint32_t segment_seq = ntohl(tcp->seq);

        if (port == 9000 && remote_port == client.remote_port && remote_inaddr == client.remote_inaddr) {
            if (client.state == TCP_STA_SYNACK_SENT) {
                kdebug("Remote side confirmed establishing\n");
                client.state = TCP_STA_ESTABLISHED;
                ++client.local_seq;
            } else if (client.state == TCP_STA_ESTABLISHED) {
                if (segment_seq < client.remote_seq) {
                    panic("TODO: handle this\n");
                }

                kdebug("Got ACK on established socket\n");
                if (tcp->flags.psh) {
                    kdebug("Also PSH\n");
                }
                uint32_t rel_seq = segment_seq - client.remote_seq;
                kdebug("Seq %u:%u\n", rel_seq, rel_seq + len);
                if (rel_seq != 0) {
                    panic("TODO: out-of-order segment reception\n");
                }
                debug_dump(DEBUG_DEFAULT, data, len);

                // Reply with ACK
                struct tcp_flags flags = {
                    .ack = 1
                };
                client.remote_seq += len;
                tcp_packet_send(p->dev, &client, flags, NULL, 0);
            }
        }
    } else {
        kwarn("%s: unhandled packet: syn=%d, ack=%d, psh=%d, rst=%d\n",
              tcp->flags.syn,
              tcp->flags.ack,
              tcp->flags.psh,
              tcp->flags.rst);
    }
}

static __init void port9000_init(void) {
    port9000.local_port = 9000;
    port9000.local_seq = 1;
    port9000.state = TCP_STA_INIT;
    port9000.type = TCP_SOCK_SERVER;
}
