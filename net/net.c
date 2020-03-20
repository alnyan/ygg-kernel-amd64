#include "arch/amd64/mm/phys.h"
#include "arch/amd64/cpu.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/sched.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/spin.h"

#include "net/packet.h"
#include "net/eth.h"
#include "net/if.h"

static struct thread netd_thread = {0};
static spin_t g_rxq_lock = 0;
static struct packet *g_rxq_head, *g_rxq_tail;

static struct packet *packet_create(void);
static void packet_free(struct packet *p);

static inline void net_handle_packet(struct packet *p) {
    // TODO: check if interface sends packet not in ethernet format
    eth_handle_frame(p);
}

static void net_daemon(void) {
    struct packet *p;
    uintptr_t irq;

    while (1) {
        if (!g_rxq_head) {
            // qword writes are atomic (when storing new head)
            // so I guess checking if ANYTHING is present
            // before locking is a good idea to avoid unnecessary
            // overhead

            thread_sleep(thread_self, system_time + 10000000ULL, NULL);
            continue;
        }

        spin_lock_irqsave(&g_rxq_lock, &irq);
        _assert(g_rxq_head);
        p = g_rxq_head;
        g_rxq_head = g_rxq_head->next;
        if (!g_rxq_head) {
            g_rxq_tail = NULL;
        } else {
            g_rxq_head->prev = NULL;
        }
        spin_release_irqrestore(&g_rxq_lock, &irq);

        net_handle_packet(p);

        packet_free(p);
    }
}

static struct packet *packet_create(void) {
    struct packet *packet;
    uintptr_t page = amd64_phys_alloc_page();
    _assert(page != MM_NADDR);
    packet = (struct packet *) MM_VIRTUALIZE(page);
    return packet;
}

static void packet_free(struct packet *p) {
    uintptr_t page = MM_PHYS(p);
    amd64_phys_free(page);
}

int net_receive(struct netdev *dev, const void *data, size_t len) {
    if (len > PACKET_DATA_MAX) {
        kwarn("%s: dropping large packet (%u)\n", dev->name, len);
        return -1;
    }
    struct packet *p = packet_create();

    // TODO: maybe information like "Physical match", "Multicast" or "Broadcast"
    //       would be useful to store in packet
    memcpy(p->data, data, len);
    p->size = len;
    p->dev = dev;

    spin_lock(&g_rxq_lock);
    p->next = NULL;
    p->prev = g_rxq_tail;
    if (g_rxq_tail) {
        g_rxq_tail->next = p;
    } else {
        g_rxq_head = p;
    }
    g_rxq_tail = p;
    spin_release(&g_rxq_lock);

    return 0;
}

void net_daemon_start(void) {
    _assert(thread_init(&netd_thread, (uintptr_t) net_daemon, NULL, 0) == 0);
    netd_thread.pid = thread_alloc_pid(0);
    sched_queue(&netd_thread);
}
