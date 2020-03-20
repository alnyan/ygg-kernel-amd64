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

struct udp_socket {
    uint32_t flags;
    uint16_t port;              // Current
    uint16_t recv_port;         // recvfrom() port
    struct thread *owner;       // Thread to suspend on blocking operation
    struct packet *pending;
};

static uint16_t udp_eph_port = 32768;

struct udp_socket *udp_socket_create(void) {
    struct udp_socket *sock = kmalloc(sizeof(struct udp_socket));
    _assert(sock);

    sock->flags = 0;
    sock->port = 0;
    sock->recv_port = 0;
    sock->owner = NULL;
    sock->pending = NULL;

    return sock;
}

int udp_socket_open(struct vfs_ioctx *ioctx, struct ofile *fd, int dom, int type, int proto) {
    struct udp_socket *sock = udp_socket_create();
    _assert(sock);

    fd->socket.family = SOCK_DGRAM;
    fd->socket.sock = sock;

    return 0;
}

ssize_t udp_socket_recv_block(struct udp_socket *sock, void *buf, size_t lim) {
    struct thread *t = thread_self;
    _assert(t);
    sock->owner = t;

    while (!sock->pending) {
        sched_unqueue(t, THREAD_WAITING_NET);
        thread_check_signal(t, 0);
    }

    return -1;
}

ssize_t udp_socket_send(struct vfs_ioctx *ioctx, struct ofile *fd, const void *buf, size_t lim, struct sockaddr *sa, size_t salen) {
    struct udp_socket *sock = fd->socket.sock;
    struct sockaddr_in *sin;
    int res;
    _assert(sock);
    _assert(sa->sa_family == AF_INET);
    sin = (struct sockaddr_in *) sa;

    if (sock->flags & UDP_SOCKET_ANY) {
        panic("TODO: sendto() from a \"listening\" socket\n");
    }

    if (!(sock->flags & UDP_SOCKET_APM)) {
        // No port allocated yet, give socket a new one
        sock->port = ++udp_eph_port;
        sock->flags |= UDP_SOCKET_APM;
    }

    // TODO: a better way of allocating these
    char packet[sizeof(struct eth_frame) + sizeof(struct inet_frame) + sizeof(struct udp_frame) + lim];
    void *data = &packet[sizeof(struct eth_frame) + sizeof(struct inet_frame) + sizeof(struct udp_frame)];
    struct udp_frame *udp = (struct udp_frame *) &packet[sizeof(struct eth_frame) + sizeof(struct inet_frame)];

    udp->src_port = htons(sock->port);
    udp->dst_port = sin->sin_port; // Already in network order
    udp->length = htons(sizeof(struct udp_frame) + lim);
    udp->checksum = 0;

    memcpy(data, buf, lim);

    struct netdev *dev = netdev_find_inaddr(ntohl(sin->sin_addr));
    if (!dev) {
        // TODO; ???
        return -ENOENT;
    }

    if ((res = inet_send_wrapped(dev, ntohl(sin->sin_addr), INET_P_UDP, packet, sizeof(packet))) != 0) {
        return res;
    }

    return lim;
}

void udp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *data, size_t len) {
    packet_ref(p);

    packet_unref(p);
}
