#include "arch/amd64/cpu.h"
#include "user/socket.h"
#include "sys/assert.h"
#include "net/packet.h"
#include "sys/thread.h"
#include "sys/string.h"
#include "net/socket.h"
#include "user/errno.h"
#include "net/class.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "net/raw.h"

static int raw_class_supports(int proto) {
    return proto == 0;
}

static int raw_socket_open(struct socket *s);
static ssize_t raw_socket_recvfrom(struct socket *s,
                                   void *buf,
                                   size_t lim,
                                   struct sockaddr *sa,
                                   size_t *salen);
static void raw_socket_close(struct socket *s);
static int raw_socket_count_pending(struct socket *s);
static struct sockops raw_socket_ops = {
    .open = raw_socket_open,
    .close = raw_socket_close,

    .sendto = NULL,
    .recvfrom = raw_socket_recvfrom,

    .bind = NULL,
    .setsockopt = NULL,

    .count_pending = raw_socket_count_pending
};
static struct socket_class raw_socket_class = {
    .name = "raw-packet",
    .ops = &raw_socket_ops,
    .domain = AF_PACKET,
    .type = SOCK_RAW,
    .supports = raw_class_supports,
};

////

struct raw_socket {
    spin_t lock;
    struct socket *gen_sock;
    struct packet_queue queue;
    struct list_head link;
};

static spin_t g_raw_lock = 0;
static LIST_HEAD(g_raw_sockets);

void raw_packet_handle(struct packet *p) {
    // Queue the packet for all the sockets
    uintptr_t irq;
    // TODO: too much time spent in spinlocked region?
    spin_lock_irqsave(&g_raw_lock, &irq);

    struct raw_socket *r;
    list_for_each_entry(r, &g_raw_sockets, link) {
        packet_ref(p);
        spin_lock(&r->lock);
        packet_queue_push(&r->queue, p);
        spin_release(&r->lock);
        _assert(r->gen_sock);
        thread_notify_io(&r->gen_sock->rx_notify);
    }
    spin_release_irqrestore(&g_raw_lock, &irq);
}

static __init void raw_class_register(void) {
    socket_class_register(&raw_socket_class);
}

//////////
// Socket implementation

static int raw_socket_open(struct socket *s) {
    // TODO: check your privilege
    uintptr_t irq;
    struct raw_socket *r_sock = kmalloc(sizeof(struct raw_socket));
    _assert(r_sock);

    packet_queue_init(&r_sock->queue);
    list_head_init(&r_sock->link);
    r_sock->gen_sock = s;
    r_sock->lock = 0;

    spin_lock_irqsave(&g_raw_lock, &irq);
    list_add(&r_sock->link, &g_raw_sockets);
    spin_release_irqrestore(&g_raw_lock, &irq);

    s->data = r_sock;

    return 0;
}

static int raw_socket_count_pending(struct socket *s) {
    struct raw_socket *sock = s->data;
    _assert(sock);
    return !!sock->queue.head;
}

static ssize_t raw_socket_recvfrom(struct socket *s,
                                   void *buf,
                                   size_t lim,
                                   struct sockaddr *sa,
                                   size_t *salen) {
    uintptr_t irq;
    int res;
    struct raw_socket *sock = s->data;
    _assert(sock);
    struct thread *t = thread_self;
    _assert(t);
    struct packet *p;

    if (!sock->queue.head) {
        res = thread_wait_io(t, &sock->gen_sock->rx_notify);

        if (res < 0) {
            return res;
        }
    }

    spin_lock_irqsave(&sock->lock, &irq);
    p = packet_queue_pop(&sock->queue);
    spin_release_irqrestore(&sock->lock, &irq);

    _assert(p);

    size_t p_size = p->size;
    if (p_size > lim) {
        kerror("Packet buffer overflow\n");
        return -1;
    }
    memcpy(buf, p->data, p_size);
    packet_unref(p);

    return p_size;
}

static void raw_socket_close(struct socket *s) {
    //uintptr_t irq;
    //struct raw_socket *r_sock = s->data;
    //_assert(r_sock);

    //spin_lock_irqsave(&g_raw_lock, &irq);
    //struct raw_socket *prev = r_sock->prev;
    //struct raw_socket *next = r_sock->next;
    //if (prev) {
    //    prev->next = next;
    //} else {
    //    g_raw_sockets = next;
    //}
    //if (next) {
    //    next->prev = prev;
    //}
    //spin_release_irqrestore(&g_raw_lock, &irq);

    //// Flush packet queue
    //while (r_sock->queue.head) {
    //    struct packet *p = packet_queue_pop(&r_sock->queue);
    //    packet_unref(p);
    //}

    //kfree(r_sock);
    s->data = NULL;
}
