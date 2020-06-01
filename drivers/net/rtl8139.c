#include "sys/mem/phys.h"
#include "arch/amd64/hw/io.h"
#include "drivers/pci/pci.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "net/packet.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "net/net.h"
#include "net/if.h"
#include "sys/mm.h"

#define CR_RST          (1 << 4)
#define CR_RE           (1 << 3)
#define CR_TE           (1 << 2)
#define CR_BUFE         (1 << 0)

#define IMR_ROK         (1 << 0)
#define IMR_RER         (1 << 1)
#define IMR_TOK         (1 << 2)
#define IMR_TER         (1 << 3)

#define ISR_ROK         (1 << 0)
#define ISR_RER         (1 << 1)
#define ISR_TOK         (1 << 2)
#define ISR_TER         (1 << 3)

#define RCR_WRAP        (1 << 7)
#define RCR_AR          (1 << 4)
#define RCR_AB          (1 << 3)            // Broadcast
#define RCR_AM          (1 << 2)            // Multicast
#define RCR_APM         (1 << 1)            // Physical Match
#define RCR_AAP         (1 << 0)            // All

#define REG_IDR(i)      ((i))
#define REG_TSD(i)      ((i) * 4 + 0x10)
#define REG_TSAD(i)     ((i) * 4 + 0x20)
#define REG_RBSTART     0x30
#define REG_CAPR        0x38
#define REG_CBR         0x3A
#define REG_CR          0x37
#define REG_IMR         0x3C
#define REG_ISR         0x3E
#define REG_RCR         0x44
#define REG_CONFIG0     0x51
#define REG_CONFIG1     0x52

struct rtl8139 {
    struct pci_device *dev;
    struct netdev *net;
    struct packet_queue tx_queue;

    int free_txds;
    uintptr_t recv_buf_phys;
    uintptr_t send_buf_pages[4];
    uint16_t rx_pos;
    size_t tx_pos;

    uint16_t iobase;
};

static void rtl8139_reset(struct rtl8139 *rtl) {
    // Power on
    outb(rtl->iobase + REG_CONFIG1, 0);

    // Software reset
    outb(rtl->iobase + REG_CR,      CR_RST);
    while (inb(rtl->iobase + REG_CR) & CR_RST);
}

static void rtl8139_send_now(struct rtl8139 *rtl, struct packet *p) {
    uintptr_t page = rtl->send_buf_pages[rtl->tx_pos];
    void *tx_buf = (void *) MM_VIRTUALIZE(page);

    memcpy(tx_buf, p->data, p->size);

    outl(rtl->iobase + REG_TSD(rtl->tx_pos), p->size & 0xFFF);
    rtl->tx_pos = (rtl->tx_pos + 1) % 4;
    --rtl->free_txds;
}

static int rtl8139_netdev_send(struct netdev *net, struct packet *p) {
    struct rtl8139 *rtl = net->device;
    _assert(rtl);
    _assert((p->size & ~0xFFF) == 0);

    if (!rtl->free_txds) {
        packet_ref(p);
        packet_queue_push(&rtl->tx_queue, p);
        kdebug("Sending too fast, queueing %p\n", p);
        return 0;
    } else {
        rtl8139_send_now(rtl, p);
    }

    return 0;
}

static uint32_t rtl8139_irq(void *ctx) {
    struct rtl8139 *rtl = ctx;
    uint16_t isr = inw(rtl->iobase + REG_ISR);
    uint32_t ret = IRQ_UNHANDLED;

    if (isr & ISR_ROK) {
        uint16_t cbr = inw(rtl->iobase + REG_CBR);
        uint16_t pkt_size;
        void *rx_buf = (void *) MM_VIRTUALIZE(rtl->recv_buf_phys);

        while ((inw(rtl->iobase + REG_CR) & CR_BUFE) == 0) {
            // Header: 4 bytes, I guess XXX
            uint16_t rx_len = ((uint16_t *) (rx_buf + rtl->rx_pos))[1];
            void *data = rx_buf + rtl->rx_pos + 4;

            if (rx_len < 4) {
                kwarn("Too small packet: %u\n", rx_len);
            } else {
                net_receive(rtl->net, data, rx_len - 4);
            }

            // Stolen this from somewhere
            rtl->rx_pos = (rtl->rx_pos + rx_len + 4 + 3) & ~3;
            outw(rtl->iobase + REG_CAPR, rtl->rx_pos - 0x10);
            rtl->rx_pos %= 0x2000;
        }

        ret = IRQ_HANDLED;
        outw(rtl->iobase + REG_ISR, ISR_ROK);
    } else if (isr & ISR_TOK) {
        ++rtl->free_txds;
        if (rtl->tx_queue.head) {
            struct packet *p = packet_queue_pop(&rtl->tx_queue);
            kdebug("Unqueueing\n");
            _assert(p);
            rtl8139_send_now(rtl, p);
            packet_unref(p);
        }

        ret = IRQ_HANDLED;
        outw(rtl->iobase + REG_ISR, ISR_TOK);
    } else {
        kinfo("???\n");
    }

    return ret;
}

static void rtl8139_init(struct pci_device *dev) {
    struct rtl8139 *rtl = kmalloc(sizeof(struct rtl8139));
    uint32_t bar0, cmd;
    uint16_t w0;
    _assert(rtl);

    rtl->dev = dev;
    rtl->rx_pos = 0;
    packet_queue_init(&rtl->tx_queue);

    // Allocate 12288 bytes (3 pages)
    rtl->recv_buf_phys = mm_phys_alloc_contiguous(3);
    _assert(rtl->recv_buf_phys != MM_NADDR);

    // Allocate Tx pages
    for (size_t i = 0; i < 4; ++i) {
        rtl->send_buf_pages[i] = mm_phys_alloc_page();
        _assert(rtl->send_buf_pages[i] != MM_NADDR);
    }
    rtl->free_txds = 4;
    rtl->tx_pos = 0;

    // Enable device bus mastering
    cmd = pci_config_read_dword(dev, PCI_CONFIG_CMD);
    cmd |= 1 << 2;
    pci_config_write_dword(dev, PCI_CONFIG_CMD, cmd);

    bar0 = pci_config_read_dword(dev, PCI_CONFIG_BAR(0));
    _assert((bar0 & 1));
    // 0xFFFF & ~0x3
    rtl->iobase = bar0 & 0xFFFC;

    rtl8139_reset(rtl);

    // Create a system network device
    struct netdev *netdev = netdev_create(IF_T_ETH);
    _assert(netdev);

    rtl->net = netdev;
    netdev->device = rtl;
    netdev->send = rtl8139_netdev_send;

    // Read MAC
    for (size_t i = 0; i < 6; ++i) {
        netdev->hwaddr[i] = inb(rtl->iobase + REG_IDR(i));
    }
    kdebug("hwaddr: %02x:%02x:%02x:%02x:%02x:%02x\n",
            netdev->hwaddr[0], netdev->hwaddr[1], netdev->hwaddr[2],
            netdev->hwaddr[3], netdev->hwaddr[4], netdev->hwaddr[5]);

    // RBSTART
    outl(rtl->iobase + REG_RBSTART, rtl->recv_buf_phys);

    // Unmask IRQs
    outw(rtl->iobase + REG_IMR, IMR_ROK | IMR_RER | IMR_TOK | IMR_TER);

    // Set RCR to receive all
    outl(rtl->iobase + REG_RCR, RCR_WRAP | RCR_AB | RCR_APM | RCR_AAP | RCR_AM);

    // Enable receiver and trasnmitter
    outb(rtl->iobase + REG_CR, CR_RE | CR_TE);
    outw(rtl->iobase + REG_ISR, 0);

    // Config Tx addresses
    for (uint16_t i = 0; i < 4; ++i) {
        outl(rtl->iobase + REG_TSAD(i), rtl->send_buf_pages[i]);
    }

    pci_add_irq(dev, rtl8139_irq, rtl);
}

static __init void rtl8139_register(void) {
    pci_add_device_driver(PCI_ID(0x10EC, 0x8139), rtl8139_init, "rtl8139");
}
