#include "arch/amd64/cpu.h"
#include "user/socket.h"
#include "user/errno.h"
#include "sys/thread.h"
#include "net/packet.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/sched.h"
#include "user/inet.h"
#include "sys/debug.h"
#include "net/util.h"
#include "sys/heap.h"
#include "fs/ofile.h"
#include "net/inet.h"
#include "net/udp.h"
#include "net/eth.h"
#include "net/if.h"

// Accept any packet (for receiving socket)
#define UDP_SOCKET_ANY      (1 << 0)
// Accept port match (when calling recvfrom)
#define UDP_SOCKET_APM      (1 << 1)
// Socket is awaiting packets
#define UDP_SOCKET_PENDING  (1 << 2)

struct udp_socket {
    uint32_t flags;
    uint16_t port;              // Current
    uint16_t recv_port;         // recvfrom() port
    uint32_t recv_inaddr;
    struct netdev *bound;       // Bound interface (all packets go to this interface if set)
    struct thread *owner;       // Thread to suspend on blocking operation
    struct packet_queue pending;
};

static uint16_t udp_eph_port = 32768;

// Only allowed for binding now
#define UDP_BIND_COUNT          16
#define UDP_BIND_START          60
static struct udp_socket *udp_ports[UDP_BIND_COUNT] = {0};

struct udp_socket *udp_socket_create(void) {
    struct udp_socket *sock = kmalloc(sizeof(struct udp_socket));
    _assert(sock);

    sock->flags = 0;
    sock->port = 0;
    sock->recv_port = 0;
    sock->owner = NULL;
    sock->bound = NULL;
    packet_queue_init(&sock->pending);

    return sock;
}

int udp_socket_open(struct vfs_ioctx *ioctx, struct ofile *fd, int dom, int type, int proto) {
    struct udp_socket *sock = udp_socket_create();
    _assert(sock);

    fd->socket.domain = AF_INET;
    fd->socket.type = SOCK_DGRAM;
    fd->socket.sock = sock;

    return 0;
}

ssize_t udp_socket_recv(struct vfs_ioctx *ioctx, struct ofile *fd, void *buf, size_t lim, struct sockaddr *sa, size_t *salen) {
    struct udp_socket *sock = fd->socket.sock;
    _assert(sock);
    struct thread *t = thread_self;
    struct packet *p;
    _assert(t);
    sock->owner = t;


    if (!sock->pending.head) {
        sock->flags |= UDP_SOCKET_PENDING;

        while ((sock->flags & UDP_SOCKET_PENDING)) {
            sched_unqueue(t, THREAD_WAITING_NET);
            thread_check_signal(t, 0);
        }
    }

    // Read a single packet
    p = packet_queue_pop(&sock->pending);
    _assert(p);

    size_t f_size = sizeof(struct eth_frame) + sizeof(struct inet_frame) + sizeof(struct udp_frame);
    const void *p_data = p->data + f_size;
    size_t p_size = p->size - f_size;

    // TODO: track position in packet so it can be read partially
    if (p_size > lim) {
        panic("Not implemented: partial packet reading\n");
    }

    memcpy(buf, p_data, p_size);
    packet_unref(p);

    return p_size;
}

ssize_t udp_socket_send(struct vfs_ioctx *ioctx, struct ofile *fd, const void *buf, size_t lim, struct sockaddr *sa, size_t salen) {
    struct udp_socket *sock = fd->socket.sock;
    struct sockaddr_in *sin;
    int res;
    uint16_t port;
    _assert(sock);
    _assert(sa->sa_family == AF_INET);
    sin = (struct sockaddr_in *) sa;

    if (sock->flags & UDP_SOCKET_ANY) {
        port = sock->recv_port;
    } else {
        if (!(sock->flags & UDP_SOCKET_APM)) {
            // No port allocated yet, give socket a new one
            sock->port = ++udp_eph_port;
            sock->flags |= UDP_SOCKET_APM;
        }

        port = sock->port;
    }

    // TODO: a better way of allocating these
    char packet[sizeof(struct eth_frame) + sizeof(struct inet_frame) + sizeof(struct udp_frame) + lim];
    void *data = &packet[sizeof(struct eth_frame) + sizeof(struct inet_frame) + sizeof(struct udp_frame)];
    struct udp_frame *udp = (struct udp_frame *) &packet[sizeof(struct eth_frame) + sizeof(struct inet_frame)];

    udp->src_port = htons(port);
    udp->dst_port = sin->sin_port; // Already in network order
    udp->length = htons(sizeof(struct udp_frame) + lim);
    udp->checksum = 0;

    memcpy(data, buf, lim);

    if (sock->bound) {
        if ((res = inet_send_wrapped(sock->bound, ntohl(sin->sin_addr), INET_P_UDP, packet, sizeof(packet))) != 0) {
            return res;
        }
    } else {
        if ((res = inet_send(ntohl(sin->sin_addr), INET_P_UDP, packet, sizeof(packet))) != 0) {
            return res;
        }
    }

    return lim;
}

int udp_socket_bind(struct vfs_ioctx *ioctx, struct ofile *fd, struct sockaddr *sa, size_t len) {
    struct udp_socket *sock = fd->socket.sock;
    struct sockaddr_in *sin;
    _assert(sa);
    _assert(sock);
    _assert(sa->sa_family == AF_INET);
    sin = (struct sockaddr_in *) sa;

    // TODO: allow binding sockets with ephemeral ports
    _assert(!(sock->flags));

    uint16_t port = ntohs(sin->sin_port);
    if (port < UDP_BIND_START || (port - UDP_BIND_START) >= UDP_BIND_COUNT) {
        return -EINVAL;
    }
    udp_ports[port - UDP_BIND_START] = sock;

    sock->flags |= UDP_SOCKET_ANY;
    sock->recv_port = port;
    sock->recv_inaddr = ntohl(sin->sin_addr);

    return 0;
}

int udp_setsockopt(struct vfs_ioctx *ioctx, struct ofile *fd, int optname, void *optval, size_t optlen) {
    struct udp_socket *sock = fd->socket.sock;
    _assert(sock);

    switch (optname) {
    case SO_BINDTODEVICE:
        // TODO: safety?
        if (!(sock->bound = netdev_by_name(optval))) {
            return -ENOENT;
        }
        return 0;
    default:
        return -EINVAL;
    }
}

void udp_socket_close(struct vfs_ioctx *ioctx, struct ofile *fd) {
    struct udp_socket *sock = fd->socket.sock;
    _assert(sock);

    sock->flags &= ~UDP_SOCKET_ANY;

    if (sock->flags & UDP_SOCKET_ANY) {
        udp_ports[sock->recv_port - UDP_BIND_START] = NULL;
    }

    kfree(sock);
    fd->socket.sock = NULL;
    fd->flags &= ~OF_SOCKET;
}

void udp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *data, size_t len) {
    struct udp_frame *udp;

    if (len < sizeof(struct udp_frame)) {
        return;
    }

    udp = data;

    uint16_t dpt = ntohs(udp->dst_port);

    if (dpt >= UDP_BIND_START && (dpt - UDP_BIND_START) < UDP_BIND_COUNT) {
        // Check if it's a packet for one of "listening" sockets
        struct udp_socket *sock = udp_ports[dpt - UDP_BIND_START];

        if (sock) {
            // Add "pending" packet for this socket
            packet_ref(p);
            packet_queue_push(&sock->pending, p);

            if (sock->flags & UDP_SOCKET_PENDING) {
                sock->flags &= ~UDP_SOCKET_PENDING;
                sched_queue(sock->owner);
            }
        } else {
            kdebug("Packet is destined to unbound port: %u\n", dpt);
        }
    }
}
