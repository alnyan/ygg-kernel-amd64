#include "arch/amd64/mm/phys.h"
#include "arch/amd64/hw/io.h"
#include "drivers/pci/pci.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "net/net.h"
#include "sys/mm.h"

#define CR_RST          (1 << 4)
#define CR_RE           (1 << 3)
#define CR_BUFE         (1 << 0)

#define IMR_ROK         (1 << 0)
#define IMR_RER         (1 << 1)

#define ISR_ROK         (1 << 0)
#define ISR_RER         (1 << 1)

#define RCR_WRAP        (1 << 7)
#define RCR_AR          (1 << 4)
#define RCR_AB          (1 << 3)            // Broadcast
#define RCR_AM          (1 << 2)            // Multicast
#define RCR_APM         (1 << 1)            // Physical Match
#define RCR_AAP         (1 << 0)            // All

#define REG_IDR(i)      ((i))
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

    uintptr_t recv_buf_phys;
    uint16_t rx_pos;

    uint8_t hwaddr[6];
    uint16_t iobase;
};

static void rtl8139_reset(struct rtl8139 *rtl) {
    // Power on
    outb(rtl->iobase + REG_CONFIG1, 0);

    // Software reset
    outb(rtl->iobase + REG_CR,      CR_RST);
    while (inb(rtl->iobase + REG_CR) & CR_RST);
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

            net_receive(rtl, data, rx_len);

            // Stolen this from somewhere
            rtl->rx_pos = (rtl->rx_pos + rx_len + 4 + 3) & ~3;
            outw(rtl->iobase + REG_CAPR, rtl->rx_pos - 0x10);
            rtl->rx_pos %= 0x2000;
        }

        ret = IRQ_HANDLED;
        outw(rtl->iobase + REG_ISR, ISR_ROK);
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

    // Allocate 12288 bytes (3 pages)
    rtl->recv_buf_phys = amd64_phys_alloc_contiguous(3);
    _assert(rtl->recv_buf_phys != MM_NADDR);

    // Enable device bus mastering
    cmd = pci_config_read_dword(dev, PCI_CONFIG_CMD);
    cmd |= 1 << 2;
    pci_config_write_dword(dev, PCI_CONFIG_CMD, cmd);

    bar0 = pci_config_read_dword(dev, PCI_CONFIG_BAR(0));
    _assert((bar0 & 1));
    // 0xFFFF & ~0x3
    rtl->iobase = bar0 & 0xFFFC;

    rtl8139_reset(rtl);

    // Read MAC
    for (size_t i = 0; i < 6; ++i) {
        rtl->hwaddr[i] = inb(rtl->iobase + REG_IDR(i));
    }
    kdebug("hwaddr: %02x:%02x:%02x:%02x:%02x:%02x\n",
            rtl->hwaddr[0], rtl->hwaddr[1], rtl->hwaddr[2],
            rtl->hwaddr[3], rtl->hwaddr[4], rtl->hwaddr[5]);

    // RBSTART
    outl(rtl->iobase + REG_RBSTART, rtl->recv_buf_phys);

    // Unmask IRQs
    outw(rtl->iobase + REG_IMR, IMR_ROK | IMR_RER);
    outw(rtl->iobase + REG_ISR, 0);

    // Set RCR to receive all
    outl(rtl->iobase + REG_RCR, RCR_WRAP | RCR_AB | RCR_APM | RCR_AAP | RCR_AM);

    // Enable receiver
    outb(rtl->iobase + REG_CR, CR_RE);

    pci_add_irq(dev, rtl8139_irq, rtl);
}

static __init void rtl8139_register(void) {
    pci_add_device_driver(PCI_ID(0x10EC, 0x8139), rtl8139_init, "rtl8139");
}
