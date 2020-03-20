#include "user/socket.h"
#include "sys/assert.h"
#include "net/packet.h"
#include "fs/ofile.h"
#include "sys/heap.h"
#include "net/raw.h"

struct raw_socket {
    struct packet_queue queue;
    struct raw_socket *prev, *next;
};

static struct raw_socket *g_raw_sockets = NULL;

int raw_socket_open(struct vfs_ioctx *ioctx, struct ofile *fd, int dom, int type, int proto) {
    // TODO: check your privilege
    struct raw_socket *r_sock = kmalloc(sizeof(struct raw_socket));
    _assert(r_sock);

    packet_queue_init(&r_sock->queue);
    r_sock->prev = NULL;
    r_sock->next = g_raw_sockets;
    g_raw_sockets = r_sock;

    fd->flags |= OF_SOCKET;
    fd->socket.sock = r_sock;
    fd->socket.domain = PF_PACKET;
    fd->socket.type = SOCK_RAW;

    return 0;
}

void raw_socket_close(struct vfs_ioctx *ioctx, struct ofile *fd) {
    // Do nothing, I guess
}

void raw_packet_handle(struct packet *p) {
    // Queue the packet for all the sockets
    for (struct raw_socket *r = g_raw_sockets; r; r = r->next) {
        packet_ref(p);
        packet_queue_push(&r->queue, p);
    }
}
