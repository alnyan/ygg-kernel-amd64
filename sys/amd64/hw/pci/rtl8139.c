#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/io.h"
#include "sys/net/eth.h"
#include "sys/net/netdev.h"
#include "sys/net/net.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "sys/mm.h"

#define RTL8139_IDR0        0x00
#define RTL8139_MAR0        0x08
#define RTL8139_TSD(n)      (0x10 + (n) * 4)
#define RTL8139_TSAD(n)     (0x20 + (n) * 4)
#define RTL8139_RBSTART     0x30
#define RTL8139_ERBCR       0x34
#define RTL8139_ERSR        0x36
#define RTL8139_CR          0x37
#define RTL8139_CAPR        0x38
#define RTL8139_CBR         0x3A
#define RTL8139_IMR         0x3C
#define RTL8139_ISR         0x3E
#define RTL8139_TCR         0x40
#define RTL8139_RCR         0x44
#define RTL8139_TCTR        0x48
#define RTL8139_MPC         0x4C
#define RTL8139_CONFIG0     0x51
#define RTL8139_CONFIG1     0x52
#define RTL8139_MSR         0x58

#define RTL8139_TSD_OWN     (1 << 13)

#define RTL8139_CR_RST      (1 << 4)
#define RTL8139_CR_RE       (1 << 3)
#define RTL8139_CR_TE       (1 << 2)

#define RTL8139_RCR_WRAP    (1 << 7)
#define RTL8139_RCR_AB      (1 << 0)
#define RTL8139_RCR_AM      (1 << 1)
#define RTL8139_RCR_APM     (1 << 2)
#define RTL8139_RCR_AAP     (1 << 3)
#define RTL8139_RCR_ALL     (RTL8139_RCR_AB | RTL8139_RCR_AM | RTL8139_RCR_APM | RTL8139_RCR_AAP)

#define RTL8139_ISR_TER     (1 << 3)
#define RTL8139_ISR_TOK     (1 << 2)
#define RTL8139_ISR_RER     (1 << 1)
#define RTL8139_ISR_ROK     (1 << 0)

#define RTL8139_IMR_TER     (1 << 3)
#define RTL8139_IMR_TOK     (1 << 2)
#define RTL8139_IMR_RER     (1 << 1)
#define RTL8139_IMR_ROK     (1 << 0)

struct rtl8139 {
    struct netdev net;

    uint32_t bar0;
    // Receive buffer: 3 pages
    uint32_t page0;
    uint32_t tx_pages[4];
    size_t tx_sizes[4];
    // Bits:
    // 0..3:    TX_BUSY Set by _tx(), cleared by _handle_tx(), means that
    //                  the descriptor cannot be allocated
    uint32_t tx_flag;
    uint32_t cbr_prev;
    uint32_t tx_number;
};

static void rtl8139_handle_rx(struct rtl8139 *rtl);
static void rtl8139_handle_tx(struct rtl8139 *rtl, int succ);

static uint32_t rtl8139_irq(void *ctx) {
    struct rtl8139 *rtl = ctx;
    uint16_t isr = inw(rtl->bar0 + RTL8139_ISR);

    if (isr) {
        if (isr & RTL8139_ISR_ROK) {
            // Got a packet
            rtl8139_handle_rx(rtl);
        }
        if (isr & (RTL8139_ISR_TOK | RTL8139_ISR_TER)) {
            rtl8139_handle_tx(rtl, isr & RTL8139_ISR_TOK);
        }

        // Implementation of RTL8139 in qemu differs
        // from the datasheet specification:
        // we need to WRITE to the register to clear the interrupts
        outw(rtl->bar0 + RTL8139_ISR, isr);

        return IRQ_HANDLED;
    }

    return IRQ_UNHANDLED;
}

static void rtl8139_handle_rx(struct rtl8139 *rtl) {
    kdebug("Received a packet\n");
    uint16_t cbr = inw(rtl->bar0 + RTL8139_CBR);
    size_t packet_size;
    uint16_t packet_offset = rtl->cbr_prev;

    if (cbr > rtl->cbr_prev) {
        packet_size = cbr - rtl->cbr_prev;
    } else {
        panic("TODO\n");
    }
    rtl->cbr_prev = cbr;

    kdebug("Packet size: %u\n", packet_size);

    char *pack = (char *) MM_VIRTUALIZE(rtl->page0) + packet_offset;
    struct eth_frame *eth_frame = (struct eth_frame *) &pack[4];

    // First 4 octets - recv status and packet length
    uint16_t recv_status = ((uint16_t *) pack)[0];
    uint16_t recv_length = ((uint16_t *) pack)[1];

    kdebug("Recv status: %u\n", recv_status);

    //kdebug("Recv length = %u\n", recv_length);
    //kdebug("Packet size = %u\n", packet_size);
    //_assert(recv_length == (packet_size - 4));

    eth_handle_frame(&rtl->net, eth_frame, recv_length);
}

static void rtl8139_cmd_tx(struct rtl8139 *rtl) {
    kdebug("HW TX %d\n", rtl->tx_number);

    uint8_t n = rtl->tx_number;

    ++rtl->tx_number;
    if (rtl->tx_number == 4) {
        rtl->tx_number = 0;
    }

    outl(rtl->bar0 + RTL8139_TSD(n), rtl->tx_sizes[n]);
}

static void rtl8139_handle_tx(struct rtl8139 *rtl, int s) {
    uint8_t prev = 3;
    if (rtl->tx_number != 0) {
        prev = rtl->tx_number - 1;
    }

    kdebug("%s Tx on %d\n", s ? "Successful" : "Failed", prev);
    kdebug("Clearing TxD%d\n", prev);
    rtl->tx_flag &= ~(1 << prev);
    rtl->tx_sizes[prev] = 0;

    kdebug("TX Done!\n");

    // If after transmission we reached a new entry and it's
    // awaiting transmission - emit a Tx command
    while (rtl->tx_flag & (1 << rtl->tx_number)) {
        rtl8139_cmd_tx(rtl);
    }
}

static uint8_t rtl8139_alloc_tx(struct rtl8139 *rtl) {
    for (uint8_t i = rtl->tx_number; i < 4; ++i) {
        if (!(rtl->tx_flag & (1 << i))) {
            return i;
        }
    }
    for (uint8_t i = 0; i < rtl->tx_number; ++i) {
        if (!(rtl->tx_flag & (1 << i))) {
            return i;
        }
    }
    return 0xFF;
}

static int rtl8139_tx(struct netdev *net, const void *packet, size_t size) {
    kdebug("Requested Tx\n");
    struct rtl8139 *rtl = (struct rtl8139 *) net;

    uint8_t index = rtl8139_alloc_tx(rtl);
    // TODO: transmit queue for this?
    assert(index != 0xFF, "No free TSADx\n");

    void *page = (void *) MM_VIRTUALIZE(rtl->tx_pages[index]);
    memcpy(page, packet, size);

    rtl->tx_sizes[index] = size;

    if (index == rtl->tx_number) {
        // Can tx right now
        rtl8139_cmd_tx(rtl);
    }

    return 0;
}

void pci_rtl8139_init(pci_addr_t addr) {
    kdebug("Initializing RTL8139 at " PCI_FMTADDR "\n", PCI_VAADDR(addr));

    struct rtl8139 *rtl = (struct rtl8139 *) kmalloc(sizeof(struct rtl8139));

    uint32_t irq_config = pci_config_read_dword(addr, PCI_CONFIG_IRQ);

    // Enable PCI busmastering
    uint32_t cmd = pci_config_read_dword(addr, PCI_CONFIG_CMD);
    cmd |= 1 << 2;
    pci_config_write_dword(addr, PCI_CONFIG_CMD, cmd);

    rtl->bar0 = pci_config_read_dword(addr, PCI_CONFIG_BAR(0));
    assert(rtl->bar0 & 1, "RTL8139 BAR0 is not in I/O space\n");
    rtl->bar0 &= ~0x3;

    // Board init
    // Set LWAKE + LWPTN to active high
    outb(rtl->bar0 + RTL8139_CONFIG1, 0);

    // Software reset
    outb(rtl->bar0 + RTL8139_CR, RTL8139_CR_RST);

    // Wait for the board to clear RST bit
    while (inb(rtl->bar0 + RTL8139_CR) & RTL8139_CR_RST);

    // Initialize rx buffer
    rtl->page0 = amd64_phys_alloc_contiguous(3);
    rtl->cbr_prev = 0;
    assert(rtl->page0 != MM_NADDR, "Failed to allocate RxBuf\n");
    outl(rtl->bar0 + RTL8139_RBSTART, rtl->page0);

    // Initialize tx buffers
    for (size_t i = 0; i < 4; ++i) {
        rtl->tx_pages[i] = amd64_phys_alloc_page();
        rtl->tx_sizes[i] = 0;
        assert(rtl->tx_pages[i] != MM_NADDR, "Failed to allocate TxBuf\n");
        outl(rtl->bar0 + RTL8139_TSAD(i), rtl->tx_pages[i]);
    }
    rtl->tx_flag = 0;
    rtl->tx_number = 0;

    // Accept all
    outl(rtl->bar0 + RTL8139_RCR, RTL8139_RCR_APM | RTL8139_RCR_AB | RTL8139_RCR_WRAP);

    // Enable Rx/Tx
    outb(rtl->bar0 + RTL8139_CR, RTL8139_CR_TE | RTL8139_CR_RE);

    uint8_t irq_pin = (irq_config >> 8) & 0xFF;
    if (irq_pin) {
        kdebug("Uses INT%c# IRQ pin\n\n", 'A' + irq_pin - 1);
        irq_add_pci_handler(addr, irq_pin - 1, rtl8139_irq, rtl);
    }

    // Configure interrupt mask
    outw(rtl->bar0 + RTL8139_IMR, RTL8139_IMR_ROK | RTL8139_IMR_RER |
                                  RTL8139_IMR_TOK | RTL8139_IMR_TER);
    // Clear ISR
    outw(rtl->bar0 + RTL8139_ISR, 0);

    // After the reset, controller uses Tx pair 0
    rtl->tx_number = 0;

    // Setup a system network device
    for (size_t i = 0; i < 6; ++i) {
        rtl->net.hwaddr[i] = inb(rtl->bar0 + RTL8139_IDR0 + i);
    }
    kdebug("RTL8139 MAC: " MAC_FMT "\n", MAC_VA(rtl->net.hwaddr));
    rtl->net.tx = rtl8139_tx;
    // TODO: call some kind of netdev_add to bind a name to the device

    // XXX: for testing
    link_add_route_in(&rtl->net, 0x0002000A, 0x00FFFFFF, 0x0102000A);
}

static __init void pci_rtl8139_register(void) {
    pci_add_device_driver(PCI_ID(0x10EC, 0x8139), pci_rtl8139_init);
}
