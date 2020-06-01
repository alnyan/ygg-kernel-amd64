#include "arch/amd64/cpu.h"
#include "user/socket.h"
#include "user/errno.h"
#include "sys/thread.h"
#include "net/packet.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "net/class.h"
#include "sys/sched.h"
#include "net/ports.h"
#include "user/inet.h"
#include "sys/debug.h"
#include "sys/wait.h"
#include "net/util.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "fs/ofile.h"
#include "net/inet.h"
#include "net/udp.h"
#include "net/eth.h"
#include "net/if.h"

static int udp_class_supports(int proto) {
    return proto == IPPROTO_UDP || proto == 0;
}

static int udp_socket_open(struct socket *sock);
static void udp_socket_close(struct socket *sock);
static ssize_t udp_socket_recvfrom(struct socket *s,
                                   void *buf,
                                   size_t lim,
                                   struct sockaddr *sa,
                                   size_t *salen);
static ssize_t udp_socket_sendto(struct socket *s,
                                 const void *buf,
                                 size_t lim,
                                 struct sockaddr *sa,
                                 size_t salen);
static int udp_socket_bind(struct socket *s, struct sockaddr *sa, size_t len);
static int udp_socket_setsockopt(struct socket *s, int optname, void *optval, size_t optlen);
static int udp_socket_count_pending(struct socket *s);

static struct sockops udp_socket_ops = {
    .open = udp_socket_open,
    .close = udp_socket_close,

    .sendto = udp_socket_sendto,
    .recvfrom = udp_socket_recvfrom,

    .bind = udp_socket_bind,
    .setsockopt = udp_socket_setsockopt,

    .count_pending = udp_socket_count_pending
};
static struct socket_class udp_socket_class = {
    .name = "udp",
    .ops = &udp_socket_ops,
    .domain = AF_INET,
    .type = SOCK_DGRAM,
    .supports = udp_class_supports,
};

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
    struct io_notify wait;
    struct packet_queue pending;
};

#define UDP_EPH_PORT_BASE           32768
static uint16_t udp_eph_port = UDP_EPH_PORT_BASE;
static struct port_array udp_port_array;

void udp_init(void) {
    port_array_init(&udp_port_array);
}

struct udp_socket *udp_socket_create(void) {
    struct udp_socket *sock = kmalloc(sizeof(struct udp_socket));
    _assert(sock);
    struct thread *t = thread_self;
    _assert(t);

    sock->flags = 0;
    sock->port = 0;
    sock->recv_port = 0;
    sock->owner = t;
    sock->bound = NULL;
    thread_wait_io_init(&sock->wait);
    packet_queue_init(&sock->pending);

    return sock;
}

void udp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *data, size_t len) {
    struct udp_frame *udp;

    if (len < sizeof(struct udp_frame)) {
        return;
    }

    udp = data;

    uint16_t dpt = ntohs(udp->dst_port);

    if (dpt < UDP_EPH_PORT_BASE) {
        struct udp_socket *sock;

        if (port_array_lookup(&udp_port_array, dpt, (void **) &sock) == 0) {
            _assert(sock);

            packet_ref(p);
            packet_queue_push(&sock->pending, p);
            thread_notify_io(&sock->wait);
        }
    }
}

static __init void udp_class_register(void) {
    socket_class_register(&udp_socket_class);
}

/////////////
// Socket impl.

static int udp_socket_open(struct socket *sock) {
    struct udp_socket *usock = udp_socket_create();
    _assert(usock);
    sock->data = usock;

    return 0;
}

static ssize_t udp_socket_recvfrom(struct socket *s,
                                   void *buf,
                                   size_t lim,
                                   struct sockaddr *sa,
                                   size_t *salen) {
    struct udp_socket *sock = s->data;
    _assert(sock);
    struct thread *t = thread_self;
    struct packet *p;
    _assert(t);
    _assert(thread_wait_io(t, &sock->wait) == 0);
    _assert(sock->pending.head);

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

    // Fill sockaddr from packet
    if (salen != NULL) {
        _assert(sa);
        struct sockaddr_in *sin = (struct sockaddr_in *) sa;
        struct inet_frame *in = (struct inet_frame *) (p->data + sizeof(struct eth_frame));
        struct udp_frame *udp = (struct udp_frame *) ((void *) in + sizeof(struct inet_frame));

        sin->sin_family = AF_INET;
        sin->sin_addr = in->src_inaddr;
        sin->sin_port = udp->src_port;

        *salen = sizeof(struct sockaddr_in);
    }

    memcpy(buf, p_data, p_size);
    packet_unref(p);

    return p_size;
}

static ssize_t udp_socket_sendto(struct socket *s,
                                 const void *buf,
                                 size_t lim,
                                 struct sockaddr *sa,
                                 size_t salen) {
    struct udp_socket *sock = s->data;
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
    struct packet *p = packet_create(PACKET_SIZE_L3INET + sizeof(struct udp_frame) + lim);
    struct udp_frame *udp = PACKET_L4(p);
    void *data = &udp[1];

    udp->src_port = htons(port);
    udp->dst_port = sin->sin_port; // Already in network order
    udp->length = htons(sizeof(struct udp_frame) + lim);
    udp->checksum = 0;

    memcpy(data, buf, lim);

    if (sock->bound) {
        if ((res = inet_send_wrapped(sock->bound, ntohl(sin->sin_addr), INET_P_UDP, p)) != 0) {
            return res;
        }
    } else {
        if ((res = inet_send(ntohl(sin->sin_addr), INET_P_UDP, p)) != 0) {
            return res;
        }
    }

    return lim;
}

static int udp_socket_bind(struct socket *s, struct sockaddr *sa, size_t len) {
    struct udp_socket *sock = s->data;
    struct sockaddr_in *sin;
    _assert(sa);
    _assert(sock);
    _assert(sa->sa_family == AF_INET);
    sin = (struct sockaddr_in *) sa;

    // TODO: allow binding sockets with ephemeral ports
    _assert(!(sock->flags));

    uint16_t port = ntohs(sin->sin_port);

    if (port >= UDP_EPH_PORT_BASE) {
        return -EINVAL;
    }

    if (port_array_lookup(&udp_port_array, port, NULL) == 0) {
        return -EEXIST;
    }

    port_array_insert(&udp_port_array, port, sock);

    sock->flags |= UDP_SOCKET_ANY;
    sock->recv_port = port;
    sock->recv_inaddr = ntohl(sin->sin_addr);

    return 0;
}

static int udp_socket_setsockopt(struct socket *s, int optname, void *optval, size_t optlen) {
    struct udp_socket *sock = s->data;
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

static void udp_socket_close(struct socket *s) {
    struct udp_socket *sock = s->data;
    _assert(sock);

    if (sock->flags & UDP_SOCKET_ANY) {
        _assert(port_array_delete(&udp_port_array, sock->recv_port) == sock);
    }

    sock->flags &= ~UDP_SOCKET_ANY;

    kfree(sock);
    s->data = NULL;
}

static int udp_socket_count_pending(struct socket *s) {
    struct udp_socket *sock = s->data;
    _assert(sock);
    return !!sock->pending.head;
}
