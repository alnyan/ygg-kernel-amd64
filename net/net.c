#include "sys/mem/phys.h"
#include "arch/amd64/cpu.h"
#include "user/socket.h"
#include "sys/thread.h"
#include "user/errno.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/sched.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"
#include "sys/spin.h"

#include "net/packet.h"
#include "net/if.h"

static struct thread netd_thread = {0};
static struct packet_queue g_rxq;

static void packet_free(struct packet *p);

static inline void net_handle_packet(struct packet *p) {
    // XXX: Dummy handler until I rewrite networking properly
}

static void net_daemon(void) {
    struct packet *p;
    uintptr_t irq;

    while (1) {
        if (!g_rxq.head) {
            // TODO: old code with sleep led to huge delays
            asm volatile ("sti; hlt");
            continue;
        }

        p = packet_queue_pop(&g_rxq);
        _assert(p);
        net_handle_packet(p);

        packet_unref(p);
    }
}

static inline struct packet_qh *packet_qh_create(struct packet *p) {
    struct packet_qh *qh = kmalloc(sizeof(struct packet_qh));
    _assert(qh);
    qh->packet = p;
    return qh;
}

void packet_queue_push(struct packet_queue *pq, struct packet *p) {
    struct packet_qh *qh = packet_qh_create(p);
    qh->next = NULL;

    uintptr_t irq;
    spin_lock_irqsave(&pq->lock, &irq);
    if (pq->tail) {
        pq->tail->next = qh;
    } else {
        pq->head = qh;
    }
    qh->prev = pq->tail;
    pq->tail = qh;
    spin_release_irqrestore(&pq->lock, &irq);
}

struct packet *packet_queue_pop(struct packet_queue *pq) {
    uintptr_t irq;
    spin_lock_irqsave(&pq->lock, &irq);
    struct packet_qh *qh = pq->head;
    _assert(qh);
    pq->head = qh->next;
    if (!pq->head) {
        pq->tail = NULL;
    }
    spin_release_irqrestore(&pq->lock, &irq);
    struct packet *p = qh->packet;
    kfree(qh);
    return p;
}

void packet_queue_init(struct packet_queue *pq) {
    pq->lock = 0;
    pq->head = NULL;
    pq->tail = NULL;
}

struct packet *packet_create(size_t size) {
    struct packet *packet;
    uintptr_t page = mm_phys_alloc_page();
    _assert(page != MM_NADDR);
    packet = (struct packet *) MM_VIRTUALIZE(page);
    packet->refcount = 0;
    packet->size = size;
    return packet;
}

static void packet_free(struct packet *p) {
    uintptr_t page = MM_PHYS(p);
    mm_phys_free_page(page);
}

void packet_ref(struct packet *p) {
    ++p->refcount;
}

void packet_unref(struct packet *p) {
    _assert(p->refcount);
    --p->refcount;
    if (!p->refcount) {
        packet_free(p);
    }
}

int net_receive(struct netdev *dev, const void *data, size_t len) {
    if (len > PACKET_DATA_MAX) {
        kwarn("%s: dropping large packet (%u)\n", dev->name, len);
        return -1;
    }
    struct packet *p = packet_create(len);

    // TODO: maybe information like "Physical match", "Multicast" or "Broadcast"
    //       would be useful to store in packet
    memcpy(p->data, data, len);
    p->dev = dev;
    packet_ref(p);

    packet_queue_push(&g_rxq, p);

    return 0;
}

void net_init(void) {
    // XXX: Dummy
}

void net_daemon_start(void) {
    packet_queue_init(&g_rxq);

    _assert(thread_init(&netd_thread, (uintptr_t) net_daemon, NULL, 0) == 0);
    netd_thread.pid = thread_alloc_pid(0);
    sched_queue(&netd_thread);
}
