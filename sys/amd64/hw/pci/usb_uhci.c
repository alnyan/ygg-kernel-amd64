#include "sys/amd64/hw/pci/usb_uhci.h"
#include "sys/amd64/hw/usb/request.h"
#include "sys/amd64/hw/usb/device.h"
#include "sys/amd64/hw/usb/usb.h"
#include "sys/amd64/hw/io.h"
#include "sys/amd64/mm/phys.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/mm.h"

#define IO_USBCMD       0x00
#define IO_USBSTS       0x02
#define IO_USBINTR      0x04
#define IO_FRNUM        0x06
#define IO_FRBASEADD    0x08
#define IO_SOFMOD       0x0C
#define IO_PORTSC1      0x10
#define IO_PORTSC2      0x12

#define USBCMD_RUN      (1 << 0)
#define USBCMD_HCRST    (1 << 1)
#define USBCMD_GRST     (1 << 2)
#define USBCMD_GSUS     (1 << 3)
#define USBCMD_GRES     (1 << 4)
#define USBCMD_SDBG     (1 << 5)
#define USBCMD_CONF     (1 << 6)
#define USBCMD_PMAX     (1 << 7)

#define USBSTS_INTR     (1 << 0)
#define USBSTS_EINT     (1 << 1)
#define USBSTS_RESD     (1 << 2)
#define USBSTS_ESYS     (1 << 3)
#define USBSTS_EPROC    (1 << 4)
#define USBSTS_HALT     (1 << 5)

#define USBINTR_TCRC    (1 << 0)
#define USBINTR_RES     (1 << 1)
#define USBINTR_DONE    (1 << 2)
#define USBINTR_SHORT   (1 << 3)

#define USBPORT_CONN    (1 << 0)
#define USBPORT_CNCH    (1 << 1)
#define USBPORT_PEN     (1 << 2)
#define USBPORT_PENCH   (1 << 3)
#define USBPORT_RESD    (1 << 6)
#define USBPORT_LSPD    (1 << 8)
#define USBPORT_RST     (1 << 9)
#define USBPORT_SUSP    (1 << 12)

#define USB_FRAME_EMPTY (1 << 0)
#define USB_FRAME_QUEUE (1 << 1)
#define USB_FRAME_FULLQ (1 << 2)

#define UHCI_MAX_QH     8
#define UHCI_MAX_TD     32

#define UHCI_TD_USED    (1 << 0)

struct uhci_td {
    uint32_t next_td;
    uint32_t status;
    uint32_t header;
    uint32_t buffer;
    union {
        uint32_t __sys0[4];
        struct {
            struct uhci_td *next;
            uint32_t flags;
        } system;
    };
} __attribute__((packed));

struct uhci_qh {
    uint32_t head_td;
    uint32_t element_td;
    // System
    uint32_t list_index;
    struct usb_transfer *transfer;
    uint32_t used;
};

struct uhci {
    struct usb_controller hc;

    uint32_t iobase;
    uint32_t *frame_list;

    uintptr_t pool_page;
    struct uhci_td *td_pool;
    struct uhci_qh *qh_pool;
    struct uhci_qh *qh_async;
};

static struct uhci_td *uhci_alloc_td(struct uhci *hc) {
    for (size_t i = 0; i < UHCI_MAX_TD; ++i) {
        if (!(hc->td_pool[i].system.flags & UHCI_TD_USED)) {
            hc->td_pool[i].system.flags |= UHCI_TD_USED;
            return &hc->td_pool[i];
        }
    }
    return NULL;
}

static struct uhci_qh *uhci_alloc_qh(struct uhci *hc) {
    for (size_t i = 0; i < UHCI_MAX_QH; ++i) {
        if (!hc->qh_pool[i].used) {
            hc->qh_pool[i].used = 1;
            return &hc->qh_pool[i];
        }
    }
    return NULL;
}

static void uhci_data_init(struct uhci *data, uint32_t bar4) {
    uintptr_t frame_list_page = amd64_phys_alloc_contiguous(2);
    _assert(frame_list_page != MM_NADDR && frame_list_page < 0x100000000);
    uintptr_t pool_page = amd64_phys_alloc_page();
    _assert(pool_page != MM_NADDR && pool_page < 0x100000000);

    data->iobase = bar4 & ~3;

    data->frame_list = (uint32_t *) MM_VIRTUALIZE(frame_list_page);
    data->pool_page = pool_page;
    data->td_pool = (struct uhci_td *) MM_VIRTUALIZE(pool_page);
    data->qh_pool = (struct uhci_qh *) (MM_VIRTUALIZE(pool_page) + sizeof(struct uhci_td) * UHCI_MAX_TD);

    memset(data->td_pool, 0, sizeof(struct uhci_td) * UHCI_MAX_TD);
    memset(data->qh_pool, 0, sizeof(struct uhci_qh) * UHCI_MAX_QH);

    data->qh_async = uhci_alloc_qh(data);
    _assert(data->qh_async);
    data->qh_async->head_td = USB_FRAME_EMPTY;
    data->qh_async->element_td = USB_FRAME_EMPTY;
    data->qh_async->transfer = NULL;

    for (uint32_t i = 0; i < 2048; ++i) {
        data->frame_list[i] = USB_FRAME_QUEUE | MM_PHYS(data->qh_async);
    }
}

static uint16_t uhci_port_reset(struct uhci *uhci, int port) {
    uint16_t r;
    uint16_t reg = uhci->iobase + port * 2 + IO_PORTSC1;

    r = inw(reg);
    r |= USBPORT_RST;
    outw(reg, r);
    for (size_t i = 0; i < 10000000; ++i);
    r = inw(reg);
    r &= ~USBPORT_RST;
    outw(reg, r);

    for (size_t i = 0; i < 10; ++i) {
        for (size_t _ = 0; _ < 10000000; ++_);

        r = inw(reg);

        // Port is not connected
        if (!(r & USBPORT_CONN)) {
            break;
        }

        if (r & (USBPORT_PENCH | USBPORT_CNCH)) {
            r &= ~(USBPORT_PENCH | USBPORT_CNCH);
            outw(reg, r);
            continue;
        }

        // Check if device is enabled
        if (r & USBPORT_PEN) {
            break;
        }

        // Try to enable the port
        r |= USBPORT_PEN;
        outw(reg, r);
    }

    return r;
}

static void uhci_start_qh(struct uhci *hc, struct uhci_qh *qh) {
    _assert(hc->qh_async->head_td == USB_FRAME_EMPTY || hc->qh_async->head_td == 0);
    hc->qh_async->head_td = (uint32_t) MM_PHYS(qh) | USB_FRAME_QUEUE;
    hc->qh_async->element_td = 0; //(uint32_t) MM_PHYS(qh) | USB_FRAME_QUEUE;
    //for (size_t i = 0; i < 1024; ++i) {
    //    //if ((hc->frame_list[i] & ~0xF) == MM_PHYS(hc->qh_async)) {
    //    //    _assert(!(MM_PHYS(qh) & 0xF));
    //    //    qh->list_index = i;
    //    //    hc->frame_list[i] = MM_PHYS(qh) | USB_FRAME_QUEUE;
    //    //    return;
    //    //}
    //}
    //panic("Failed to allocate a frame for USB transfer\n");
}

static void uhci_wait_qh(struct uhci *hc, struct uhci_qh *qh) {
    struct usb_transfer *t = qh->transfer;
    while (!(t->flags & USB_TRANSFER_COMPLETE)) {
        uint32_t elem_phys = qh->element_td & ~0xF;

        if (!elem_phys) {
            t->flags = USB_TRANSFER_COMPLETE | USB_TRANSFER_SUCCESS;
            break;
        } else {
            struct uhci_td *td = (struct uhci_td *) MM_VIRTUALIZE(elem_phys);

            if (td->status & (1 << 22)) {
                kdebug("Transfer descriptor is stalled\n");
                t->flags = USB_TRANSFER_COMPLETE;
                break;
            }
        }
    }
    hc->qh_async->head_td = USB_FRAME_EMPTY;
    uint32_t td_ptr = (qh->head_td & ~0xF);
    _assert(td_ptr);

    do {
        if (!td_ptr) {
            break;
        }

        struct uhci_td *td = (struct uhci_td *) MM_VIRTUALIZE(td_ptr);
        if (td->next_td == USB_FRAME_EMPTY || !td->next_td) {
            td->next_td = USB_FRAME_EMPTY;
            td->buffer = 0;
            td->system.flags &= ~UHCI_TD_USED;
            break;
        }

        td_ptr = td->next_td & ~0xF;
        _assert(td_ptr);

        td->system.flags &= ~UHCI_TD_USED;
        td->next_td = USB_FRAME_EMPTY;
        td->buffer = 0;
    } while (1);

    // TODO: Properly release the queue
    qh->transfer = NULL;
    qh->head_td = USB_FRAME_EMPTY;
    qh->element_td = USB_FRAME_EMPTY;
    qh->used = 0;

    kdebug("Transfer is complete\n");
}

static void uhci_init_td(struct uhci_td *td, struct uhci_td *prev, uint8_t speed, uint8_t addr, uint8_t endp, uint16_t len, uint8_t packet_type, uint8_t toggle, void *buf) {
    // Packet header:
    // 0 .. 7           Packet type
    //                  0x69 IN
    //                  0xE1 OUT
    //                  0x2D SETUP
    // 8 .. 14          Device address
    // 15 .. 18         Endpoint address
    // 19               Data toggle
    // 20               Reserved (0)
    // 21 .. 31         Maximum payload length - 1
    _assert((((uintptr_t) td) & 0xF) == 0);
    len = (len - 1) & 0x7FF;

    if (prev) {
        prev->next_td = (uint32_t) MM_PHYS(td) | USB_FRAME_FULLQ;
    }

    td->header = packet_type |
                 (len << 21) |
                 (toggle << 19) |
                 (addr << 8) |
                 (endp << 15);

    // Status:
    // 23               Active
    // 26               Low speed
    // 27 .. 28         Error counter
    td->status = (1 << 23) |
                 (3 << 27);

    if (speed == USB_SPEED_LOW) {
        td->status |= 1 << 26;
    }

    td->buffer = MM_PHYS(buf);
}

static void uhci_device_control(struct usb_device *dev, struct usb_transfer *t) {
    struct uhci *hc = dev->hc;
    struct usb_request *req = t->request;
    uint8_t max_len = sizeof(struct usb_request);

    struct uhci_td *td, *head, *prev;
    struct uhci_qh *qh;

    td = uhci_alloc_td(hc);
    _assert(td);
    head = td;

    uint8_t addr = dev->address;
    uint8_t endp = 0;
    uint8_t speed = dev->speed;
    uint16_t packet_size = sizeof(struct usb_request);
    uint8_t packet_type = 0x2D;
    uint8_t toggle = 0;

    // Setup first packet (SETUP)
    uhci_init_td(td, NULL, speed, addr, endp, packet_size, packet_type, toggle, req);
    prev = td;

    // Middle packets (IN)
    size_t off = 0;
    size_t rem = req->length;
    packet_type = (req->type & USB_REQUEST_TYPE_D2H) ? 0x69 : 0xE1;
    while (rem) {
        packet_size = MIN(rem, dev->max_packet);
        td = uhci_alloc_td(hc);
        _assert(td);

        toggle ^= 1;
        uhci_init_td(td, prev, speed, addr, endp, packet_size, packet_type, toggle, t->data + off);

        prev = td;
        off += packet_size;
        rem -= packet_size;
    }

    // Ending packet (OUT)
    td = uhci_alloc_td(hc);
    _assert(td);

    packet_type = (req->type & USB_REQUEST_TYPE_D2H) ? 0xE1 : 0x69;
    uhci_init_td(td, prev, speed, addr, endp, 0, packet_type, 1, NULL);

    qh = uhci_alloc_qh(hc);
    _assert(qh);
    qh->head_td = (uint32_t) MM_PHYS(head) | USB_FRAME_FULLQ;
    qh->element_td = (uint32_t) MM_PHYS(head) | USB_FRAME_FULLQ;
    qh->transfer = t;

    uhci_start_qh(hc, qh);
    uhci_wait_qh(hc, qh);
}

static void uhci_probe(struct uhci *uhci) {
    uint16_t portsc;

    for (int port = 0; port < 2; ++port) {
        portsc = uhci_port_reset(uhci, port);

        if (portsc & USBPORT_PEN) {
            kdebug("Port %d is enabled\n", port);

            // Setup a new USB device
            struct usb_device *dev = usb_device_create();
            dev->hc = uhci;
            dev->max_packet = 8;
            dev->hc_control = uhci_device_control;
            dev->address = 0;

            if (portsc & USBPORT_LSPD) {
                dev->speed = USB_SPEED_LOW;
            } else {
                dev->speed = USB_SPEED_FULL;
            }

            usb_device_init(dev);
        }
    }
}

void pci_usb_uhci_init(pci_addr_t addr) {
    uint32_t bar4;
    struct uhci *hc = kmalloc(sizeof(struct uhci));
    _assert(hc);

    kdebug("Initializing USB UHCI at " PCI_FMTADDR "\n", PCI_VAADDR(addr));

    bar4 = pci_config_read_dword(addr, PCI_CONFIG_BAR(4));
    _assert(bar4 & 1);
    uhci_data_init(hc, bar4);
    kdebug("Base addr %p\n", hc->frame_list);

    // Disable IRQs
    outw(hc->iobase + IO_USBINTR, 0);
    // Setup frame lists
    outl(hc->iobase + IO_FRBASEADD, (uint32_t) MM_PHYS(hc->frame_list));
    outw(hc->iobase + IO_FRNUM, 0);
    outw(hc->iobase + IO_SOFMOD, 0x40);
    // Clear status
    outw(hc->iobase + IO_USBSTS, 0xFFFF);
    // Enable controller
    outw(hc->iobase + IO_USBCMD, USBCMD_RUN);

    uhci_probe(hc);

    hc->hc.spec = USB_SPEC_UHCI;

    usb_controller_add((struct usb_controller *) hc);
}
