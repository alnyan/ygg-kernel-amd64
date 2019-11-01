#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/io.h"
#include "sys/net/eth.h"
#include "sys/net/net.h"
#include "sys/assert.h"
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
    uint32_t bar0;
    // Receive buffer: 3 pages
    uint32_t page0;
    uint32_t cbr_prev;
};

static void rtl8139_handle_recv(struct rtl8139 *rtl);

static uint32_t rtl8139_irq(void *ctx) {
    struct rtl8139 *rtl = ctx;
    uint16_t isr = inw(rtl->bar0 + RTL8139_ISR);

    if (isr) {
        if (isr & RTL8139_ISR_ROK) {
            // Got a packet
            rtl8139_handle_recv(rtl);
        }

        // Implementation of RTL8139 in qemu differs
        // from the datasheet specification:
        // we need to WRITE to the register to clear the interrupts
        outw(rtl->bar0 + RTL8139_ISR, isr);

        return IRQ_HANDLED;
    }

    return IRQ_UNHANDLED;
}

static void rtl8139_handle_recv(struct rtl8139 *rtl) {
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

    eth_handle_frame(rtl, eth_frame, packet_size);
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
    outw(rtl->bar0 + RTL8139_IMR, RTL8139_IMR_ROK | RTL8139_IMR_RER);
    // Clear ISR
    outw(rtl->bar0 + RTL8139_ISR, 0);

    // Dump contents of MAC register
    uint8_t mac[6];
    for (size_t i = 0; i < 6; ++i) {
        mac[i] = inb(rtl->bar0 + RTL8139_IDR0 + i);
    }
    kdebug("RTL8139 MAC: " MAC_FMT "\n", MAC_VA(mac));
}

static __init void pci_rtl8139_register(void) {
    pci_add_device_driver(PCI_ID(0x10EC, 0x8139), pci_rtl8139_init);
}
