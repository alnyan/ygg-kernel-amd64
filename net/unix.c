#include "arch/amd64/cpu.h"
#include "sys/char/ring.h"
#include "user/socket.h"
#include "user/errno.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "net/socket.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "user/stat.h"
#include "net/class.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "user/un.h"
#include "fs/node.h"
#include "fs/vfs.h"

// TODO: check for possible races
// TODO: SOCK_DGRAM
static int unix_class_supports(int proto) {
    return proto == 0;
}

static int unix_socket_open(struct socket *sock);
static void unix_socket_close(struct socket *sock);
static int unix_socket_bind(struct socket *sock, struct sockaddr *sa, size_t len);
static int unix_socket_connect(struct socket *sock, struct sockaddr *sa, size_t len);
static int unix_socket_accept(struct socket *serv, struct socket *client);
static ssize_t unix_socket_sendto(struct socket *s,
                                  const void *buf, size_t lim,
                                  struct sockaddr *dst, size_t salen);
static ssize_t unix_socket_recvfrom(struct socket *s,
                                    void *buf, size_t lim,
                                    struct sockaddr *dst, size_t *salen);

static struct sockops unix_socket_ops = {
    .open =     unix_socket_open,
    .close =    unix_socket_close,

    .sendto =   unix_socket_sendto,
    .recvfrom = unix_socket_recvfrom,

    .bind =     unix_socket_bind,
    .connect =  unix_socket_connect,

    .accept =   unix_socket_accept,
};
static struct socket_class unix_socket_class = {
    .name =     "unix",
    .ops =      &unix_socket_ops,
    .domain =   AF_UNIX,
    .type =     SOCK_STREAM,
    .supports = unix_class_supports
};

static ssize_t unix_conn_sendto(struct socket *s,
                                const void *buf, size_t lim,
                                struct sockaddr *dst, size_t salen);
static ssize_t unix_conn_recvfrom(struct socket *s,
                                  void *buf, size_t lim,
                                  struct sockaddr *dst, size_t *salen);
static void unix_conn_close(struct socket *s);

static struct sockops unix_conn_ops = {
    .sendto =   unix_conn_sendto,
    .recvfrom = unix_conn_recvfrom,
    .close =    unix_conn_close
};

////

enum unix_conn_state {
    STATE_NEW = 1,
    STATE_PRE_ESTABLISHED,
    STATE_ESTABLISHED,
    STATE_TERMINATED
};

struct unix_socket {
    int type;

    // Shared vnode
    struct vnode *vnode;

    // If server:
    //   receive notify when client attempts connections
    // If client:
    //   receive notify when server acknowledges connection
    struct io_notify client_notify;

    // If server
    size_t remote_count;

    // If server:
    //  attempt of remote connection
    // If client:
    //  remote side
    struct unix_conn *remote;
};

struct unix_conn {
    struct unix_socket *server;
    struct unix_socket *client;

    int state;

    struct ring client_tx;
    struct ring server_tx;
};

static int unix_socket_open(struct socket *sock) {
    struct unix_socket *data = kmalloc(sizeof(struct unix_socket));
    _assert(data);
    sock->data = data;
    data->type = 0;
    data->remote = NULL;
    data->remote_count = 0;
    thread_wait_io_init(&data->client_notify);
    return 0;
}

static void unix_socket_close(struct socket *sock) {
    struct unix_socket *data = sock->data;
    _assert(data);

    if (data->type == 1) {
        kdebug("Closing with remote_count = %d\n", data->remote_count);
        _assert(data->remote_count == 0);
        struct vnode *vn = data->vnode;
        _assert(vn);
        vn->fs_data = NULL;
    } else {
        // Hangup connection
        if (data->remote) {
            struct unix_conn *conn = data->remote;
            // That's us
            _assert(conn->client);

            if (conn->state == STATE_ESTABLISHED) {
                conn->state = STATE_TERMINATED;
                // Send EOF to remote
                _assert(conn->server);
                ring_signal(&conn->client_tx, RING_SIGNAL_EOF);
            }
            conn->client = NULL;

            if (!conn->server) {
                kinfo("Client side removes the connection\n");
                memset(conn, 0, sizeof(struct unix_conn));
                kfree(conn);
            }
        }
    }

    kfree(data);
}

static void unix_conn_close(struct socket *s) {
    struct unix_conn *conn = s->data;
    _assert(conn);

    // That's us
    _assert(conn->server);

    if (conn->state == STATE_ESTABLISHED) {
        conn->state = STATE_TERMINATED;
        // Send EOF to remote
        _assert(conn->client);
        ring_signal(&conn->server_tx, RING_SIGNAL_EOF);
        conn->server = NULL;
    }

    if (!conn->client) {
        _assert(conn->server->remote_count > 0);
        --conn->server->remote_count;

        kinfo("Server side removes the connection\n");
        memset(conn, 0, sizeof(struct unix_conn));
        kfree(conn);
    }
}

static int unix_socket_bind(struct socket *sock, struct sockaddr *sa, size_t len) {
    if (sa->sa_family != AF_UNIX) {
        return -EINVAL;
    }
    struct sockaddr_un *sun = (struct sockaddr_un *) sa;
    int res;

    struct unix_socket *data = sock->data;
    _assert(data);

    // If socket already exists
    if ((res = vfs_find(&thread_self->proc->ioctx, NULL, sun->sun_path, 0, &data->vnode)) == 0) {
        data->vnode = NULL;
        return -EADDRINUSE;
    }

    // Create socket vnode
    if ((res = vfs_mknod(&thread_self->proc->ioctx, sun->sun_path, 0777 | S_IFSOCK, &data->vnode)) != 0) {
        return res;
    }

    // Setup server socket params
    data->type = 1;
    data->vnode->fs_data = data;

    return 0;
}

static int unix_socket_connect(struct socket *sock, struct sockaddr *sa, size_t len) {
    if (sa->sa_family != AF_UNIX) {
        return -EINVAL;
    }
    struct sockaddr_un *sun = (struct sockaddr_un *) sa;
    struct unix_socket *data = sock->data;
    int res;
    _assert(data);

    if ((res = vfs_find(&thread_self->proc->ioctx, NULL, sun->sun_path, 0, &data->vnode)) != 0) {
        return -ENOENT;
    }

    if (!data->vnode->fs_data) {
        return -ECONNREFUSED;
    }

    struct unix_conn *conn = kmalloc(sizeof(struct unix_conn));
    conn->client = data;
    conn->server = data->vnode->fs_data;
    conn->state = STATE_NEW;
    ring_init(&conn->client_tx, 1024);
    ring_init(&conn->server_tx, 1024);

    // Notify server of connection
    _assert(!conn->server->remote);
    conn->server->remote = conn;
    thread_notify_io(&conn->server->client_notify);

    // Await server connection ack
    while ((conn->state == STATE_NEW)) {
        thread_wait_io(thread_self, &data->client_notify);
    }
    _assert(conn->state == STATE_PRE_ESTABLISHED);
    data->remote = conn;

    // Confirm we're ready
    conn->state = STATE_ESTABLISHED;
    thread_notify_io(&conn->server->client_notify);

    return 0;
}

static int unix_socket_accept(struct socket *serv, struct socket *client) {
    struct unix_socket *data_serv;
    struct unix_conn *conn;
    data_serv = serv->data;
    _assert(data_serv);

    // Wait for incoming connection attempts
    _assert(!(data_serv->remote));
    while (!(conn = data_serv->remote)) {
        thread_wait_io(thread_self, &data_serv->client_notify);
    }
    data_serv->remote = NULL;

    // Setup client socket
    client->data = conn;
    client->op = &unix_conn_ops;
    client->ioctx = serv->ioctx;
    ++data_serv->remote_count;

    // Acknowledge connection attempt
    conn->state = STATE_PRE_ESTABLISHED;
    thread_notify_io(&conn->client->client_notify);

    // Acknowledge client ready to prevent server from terminating
    // connection too early
    while (conn->state == STATE_PRE_ESTABLISHED) {
        thread_wait_io(thread_self, &data_serv->client_notify);
    }

    return 0;
}

static ssize_t unix_ring_recv(struct ring *r, void *buf, size_t lim) {
    size_t rd = 0;
    char c;
    size_t rem = lim;
    char *wr = buf;

    while (rem) {
        if (ring_getc(thread_self, r, &c, 0) < 0) {
            break;
        }

        *wr++ = c;
        ++rd;
        --rem;

        if (!ring_readable(r) && (r->flags & RING_SIGNAL_RET)) {
            r->flags &= ~RING_SIGNAL_RET;
            return rd;
        }
    }

    return rd;
}

static ssize_t unix_socket_sendto(struct socket *s,
                                  const void *buf, size_t lim,
                                  struct sockaddr *dst, size_t salen) {
    struct unix_socket *data = s->data;
    _assert(data);
    _assert(data->type == 0);
    struct unix_conn *conn = data->remote;
    _assert(conn);
    if (conn->state != STATE_ESTABLISHED) {
        return -ECONNRESET;
    }

    ssize_t res = ring_write(thread_self, &conn->client_tx, buf, lim, 1);
    ring_signal(&conn->client_tx, RING_SIGNAL_RET);

    return res;
}

static ssize_t unix_socket_recvfrom(struct socket *s,
                                    void *buf, size_t lim,
                                    struct sockaddr *dst, size_t *salen) {
    struct unix_socket *data = s->data;
    _assert(data);
    _assert(data->type == 0);
    struct unix_conn *conn = data->remote;
    _assert(conn);
    if (conn->state != STATE_ESTABLISHED && !ring_readable(&conn->server_tx)) {
        return -ECONNRESET;
    }
    return unix_ring_recv(&conn->server_tx, buf, lim);
}

static ssize_t unix_conn_sendto(struct socket *s,
                                const void *buf, size_t lim,
                                struct sockaddr *dst, size_t salen) {
    struct unix_conn *conn = s->data;
    _assert(conn);
    if (conn->state != STATE_ESTABLISHED) {
        return -ECONNRESET;
    }

    ssize_t res = ring_write(thread_self, &conn->server_tx, buf, lim, 1);
    ring_signal(&conn->server_tx, RING_SIGNAL_RET);

    return res;
}

static ssize_t unix_conn_recvfrom(struct socket *s,
                                  void *buf, size_t lim,
                                  struct sockaddr *dst, size_t *salen) {
    struct unix_conn *conn = s->data;
    _assert(conn);
    if (conn->state != STATE_ESTABLISHED && !ring_readable(&conn->client_tx)) {
        return -ECONNRESET;
    }
    return unix_ring_recv(&conn->client_tx, buf, lim);
}

////

__init(unix_class_register) {
    socket_class_register(&unix_socket_class);
}
