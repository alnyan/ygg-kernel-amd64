#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/hw/ioapic.h"
#include "sys/amd64/hw/io.h"
#include "sys/panic.h"
#include "sys/assert.h"
#include "sys/debug.h"

#include "sys/amd64/hw/pci/ide.h"

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    uint32_t w0;
    w0 = ((uint32_t) bus << 16) |
         ((uint32_t) slot << 11) |
         ((uint32_t) func << 8) |
         ((uint32_t) off & ~0x3) |
         (1 << 31);

    outl(PCI_PORT_CONFIG_ADDR, w0);
    w0 = inl(PCI_PORT_CONFIG_DATA);

    return w0;
}

void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t v) {
    uint32_t w0;
    w0 = ((uint32_t) bus << 16) |
         ((uint32_t) slot << 11) |
         ((uint32_t) func << 8) |
         ((uint32_t) off & ~0x3) |
         (1 << 31);

    outl(PCI_PORT_CONFIG_ADDR, w0);
    outl(PCI_PORT_CONFIG_DATA,v);
}

#define PCI_MAX_ROOT_BUSES      4
#define PCI_MAX_BRIDGE_BUSES    8

static uint8_t pci_root_buses[PCI_MAX_ROOT_BUSES] = {0};
static size_t pci_root_bus_count = 0;

static struct pci_bridge_bus {
    uint8_t bridge_bus, bridge_dev, bridge_func;    // Bridge device address on root bus
    uint8_t bus_number;                             // Secondary bus number
} pci_bridge_buses[PCI_MAX_BRIDGE_BUSES] = {0};
static size_t pci_bridge_bus_count = 0;

void pci_add_root_bus(uint8_t n) {
    if (pci_root_bus_count == PCI_MAX_ROOT_BUSES) {
        panic("Too many PCI root buses on host\n");
    }

    pci_root_buses[pci_root_bus_count++] = n;
}

void pci_add_bridge_bus(uint8_t bbus, uint8_t bdev, uint8_t bfunc, uint8_t sbus) {
    if (pci_bridge_bus_count == PCI_MAX_BRIDGE_BUSES) {
        panic("Too many PCI bridge buses on host\n");
    }

    struct pci_bridge_bus *bus = &pci_bridge_buses[pci_bridge_bus_count++];

    bus->bridge_bus = bbus;
    bus->bridge_dev = bdev;
    bus->bridge_func = bfunc;
    bus->bus_number = sbus;
}

// If NULL is returned then `bus` is PCI root bus,
// otherwise it's a PCI-to-PCI bridged bus
static struct pci_bridge_bus *pci_bus_bridge(uint8_t bus) {
    for (size_t i = 0; i < pci_bridge_bus_count; ++i) {
        if (bus == pci_bridge_buses[i].bus_number) {
            return &pci_bridge_buses[i];
        }
    }

    return NULL;
}

static void pci_describe_func(uint8_t bus, uint8_t dev, uint8_t func) {
    // Get device actual bus
    struct pci_bridge_bus *bridge = pci_bus_bridge(bus);

    uint32_t header = pci_config_read_dword(bus, dev, func, 0x0C);
    uint8_t header_type = (header >> 16) & 0x7F;

    if (header_type == 0) {
        uint32_t id = pci_config_read_dword(bus, dev, func, 0);
        uint32_t kind = pci_config_read_dword(bus, dev, func, 8);
        uint32_t irq = pci_config_read_dword(bus, dev, func, 0x3C);

        uint16_t class = kind >> 16;

        uint16_t device_id = id >> 16;
        uint16_t vendor_id = id & 0xFFFF;

        kdebug("[%02x:%02x:%02x] %02x:%02x/%s\n",
            bus, dev, func,
            class >> 8, class & 0xFF,
            pci_class_string(class));
        kdebug(" Vendor %04x Device %04x\n", vendor_id, device_id);

        if (class == 0x0C03) {
            // Describe actual USB controller type
            switch ((kind >> 8) & 0xFF) {
            case 0x00:
                kdebug(" UHCI controller (USB 1)\n");
                break;
            case 0x10:
                kdebug(" OHCI controller (USB 1)\n");
                break;
            case 0x20:
                kdebug(" EHCI controller (USB 2)\n");
                break;
            case 0x30:
                kdebug(" XHCI controller (USB 3)\n");
                break;
            case 0xFF:
                kdebug(" USB device\n");
                break;
            }
        }

        uint8_t root_bus = bus;
        uint8_t root_dev = dev;
        uint8_t root_func = func;

        if (bridge) {
            root_bus = bridge->bridge_bus;
            root_dev = bridge->bridge_dev;
            root_func = bridge->bridge_func;
        }

        if (irq & 0xFF00) {
            uint8_t pin = (irq >> 8) - 1;

            kdebug("  Interrupt pin INT%c#\n", pin + 'A');
            // Dump current route
            uint32_t gsi_irq = amd64_pci_pin_irq_route(root_bus, root_dev, root_func, pin);
            _assert(gsi_irq != PCI_IRQ_INVALID); // TODO: allow dynamic PCI IRQ route allocation

            if (gsi_irq != PCI_IRQ_NO_ROUTE) {
                kdebug("    -> I/O APIC IRQ #%d\n", gsi_irq);
            } else {
                kdebug("    -> Unmapped I/O APIC IRQ\n");
            }
        }
        // Otherwise the device either does not use IRQs or uses legacy IRQs?
        // This case should be handled by device driver
    } else if (header_type == 1) {
        kdebug("[%02x:%02x:%02x] PCI-to-PCI Bridge\n", bus, dev, func);

        assert(!bridge, "Nested bridged buses?\n");

        uint32_t bridge_bus_info = pci_config_read_dword(bus, dev, func, 0x18);
        uint8_t secondary_bus_number = (bridge_bus_info >> 8) & 0xFF;
        uint8_t primary_bus_number = bridge_bus_info & 0xFF;

        kdebug("    Primary bus: %02x\n", primary_bus_number);
        kdebug("    Secondary bus: %02x\n", secondary_bus_number);

        pci_add_bridge_bus(bus, dev, func, secondary_bus_number);
    } else {
        panic("[%02x:%02x:%02x]: Unsupported PCI header type: %02x\n",
            bus, dev, func,
            header_type);
    }
}

static void pci_enumerate_func(uint8_t bus, uint8_t dev, uint8_t func) {
    // Get device actual bus
    struct pci_bridge_bus *bridge = pci_bus_bridge(bus);
    uint32_t header = pci_config_read_dword(bus, dev, func, 0x0C);
    uint8_t header_type = (header >> 16) & 0x7F;

    pci_describe_func(bus, dev, func);

    if (header_type == 0) {
        uint32_t id = pci_config_read_dword(bus, dev, func, 0);
        uint32_t kind = pci_config_read_dword(bus, dev, func, 8);

        uint16_t class = kind >> 16;

        uint16_t device_id = id >> 16;
        uint16_t vendor_id = id & 0xFFFF;

        switch (class) {
        case 0x0101:
            // PCI IDE Controller
            pci_ide_init(bus, dev, func);
            break;
        }
    } else if (header_type == 1) {
        kdebug("[%02x:%02x:%02x] PCI-to-PCI Bridge\n", bus, dev, func);

        assert(!bridge, "Nested bridged buses?\n");

        uint32_t bridge_bus_info = pci_config_read_dword(bus, dev, func, 0x18);
        uint8_t secondary_bus_number = (bridge_bus_info >> 8) & 0xFF;
        uint8_t primary_bus_number = bridge_bus_info & 0xFF;

        kdebug("    Primary bus: %02x\n", primary_bus_number);
        kdebug("    Secondary bus: %02x\n", secondary_bus_number);

        pci_add_bridge_bus(bus, dev, func, secondary_bus_number);
    } else {
        panic("[%02x:%02x:%02x]: Unsupported PCI header type: %02x\n",
            bus, dev, func,
            header_type);
    }
}

static void pci_enumerate_dev(uint8_t bus, uint8_t dev) {
    // Check function 0
    uint32_t w0 = pci_config_read_dword(bus, dev, 0, 0);
    if ((w0 & 0xFFFF) == 0xFFFF) {
        // No functions here
        return;
    }
    uint32_t header = pci_config_read_dword(bus, dev, 0, 0x0C);

    pci_enumerate_func(bus, dev, 0);

    if ((header >> 16) & 0x80) {
        // Multifunction device
        for (uint8_t i = 1; i < 8; ++i) {
            if ((pci_config_read_dword(bus, dev, i, 0x00) & 0xFFFF) != 0xFFFF) {
                pci_enumerate_func(bus, dev, i);
            }
        }
    }
}

void pci_init(void) {
    // Initialize IRQ routing first
    if (amd64_pci_init_irqs() != 0) {
        panic("PCI: Failed to initialize IRQs\n");
    }

    // Enumerate root buses
    // TODO: check PCI 0:0:0 properly
    //       if it's a multifunction device there are several root buses
    //       and they should be handled properly
    for (size_t i = 0; i < pci_root_bus_count; ++i) {
        uint8_t bus = pci_root_buses[i];

        for (uint8_t dev = 0; dev < 32; ++dev) {
            pci_enumerate_dev(bus, dev);
        }
    }

    // Enumerate bridged buses
    for (size_t i = 0; i < pci_bridge_bus_count; ++i) {
        uint8_t bus = pci_bridge_buses[i].bus_number;

        for (uint8_t dev = 0; dev < 32; ++dev) {
            pci_enumerate_dev(bus, dev);
        }
    }
}
