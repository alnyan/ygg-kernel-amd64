#include "sys/amd64/hw/ioapic.h"
#include "sys/driver/pci/pci.h"
#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/io.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/mm.h"

#define PCI_MAX_DRIVERS         64

#define pcie_config_read_dword(dev, off) \
    (*(uint32_t *) ((dev)->pcie_config + (off)))

#define PCI_CAP_MSI_64          (1 << 7)
#define PCI_CAP_MSI_EN          (1 << 0)
struct pci_cap_msi {
    uint8_t cap_id;
    uint8_t cap_link;
    uint16_t message_control;
    union {
        struct {
            uint32_t message_address;
            uint16_t message_data;
        } __attribute__((packed)) msi32;
        struct {
            uint64_t message_address;
            uint16_t message_data;
        } __attribute__((packed)) msi64;
    };
} __attribute__((packed));

struct pci_device {
    // PCI address
    uint8_t bus;
    uint8_t dev;
    uint8_t func;

    // PCIe addressing: segment group number and configuration space pointer
    uint16_t pcie_segment_group;
    void *pcie_config;

    // Interrupt resources
    struct pci_cap_msi *msi;
    int irq_pin;

    // For list
    struct pci_device *next;
};

struct pci_driver {
    pci_driver_func_t init_func;
    uint32_t type;
    uint32_t match;
};

static struct pci_device *g_pci_devices = NULL;
static struct pci_driver  g_pci_drivers[PCI_MAX_DRIVERS] = {0};
static size_t             g_pci_driver_count;

#define PCI_DRIVER_CLASS        1
#define PCI_DRIVER_DEV          2

uint32_t pci_config_read_dword(struct pci_device *dev, uint16_t off) {
    // TODO: check if device is really PCIe
    return pcie_config_read_dword(dev, off);
}

uint32_t pci_config_read_dword_legacy(uint8_t bus, uint8_t dev, uint8_t func, uint32_t off) {
    uint32_t w0;
    w0 = (((uint32_t) bus) << 16) |
         (((uint32_t) dev) << 11) |
         (((uint32_t) func) << 8) |
         (off & ~0x3) |
         (1 << 31);

    outl(PCI_PORT_CONFIG_ADDR, w0);
    w0 = inl(PCI_PORT_CONFIG_DATA);

    return w0;
}

void pci_add_irq(struct pci_device *dev, irq_handler_func_t handler, void *ctx) {
    if (dev->msi) {
        uint8_t vector;

        if (irq_add_msi_handler(handler, ctx, &vector) != 0) {
            panic("Failed to add MSI handler\n");
        }

        if (dev->msi->message_control & PCI_CAP_MSI_64) {
            dev->msi->msi64.message_address = 0xFEE00000;
            dev->msi->msi64.message_data = vector;
        } else {
            dev->msi->msi32.message_address = 0xFEE00000;
            dev->msi->msi32.message_data = vector;
        }
        dev->msi->message_control |= PCI_CAP_MSI_EN;
    }
}

void pci_add_class_driver(uint32_t full_class, pci_driver_func_t func) {
    if (g_pci_driver_count == PCI_MAX_DRIVERS) {
        panic("Too many PCI drivers loaded\n");
    }

    struct pci_driver *driver = &g_pci_drivers[g_pci_driver_count++];

    driver->init_func = func;
    driver->type = PCI_DRIVER_CLASS;
    driver->match = full_class & ~0xFF000000;
}

void pci_add_root_bus(uint8_t n) {
    kwarn("%s: %02x\n", __func__, n);
}

static void pci_pick_driver(uint32_t device_id, uint32_t class_id, pci_driver_func_t *class_driver, pci_driver_func_t *dev_driver) {
    pci_driver_func_t rclass = NULL, rdev = NULL;
    for (size_t i = 0; i < g_pci_driver_count; ++i) {
        if (rclass && rdev) {
            break;
        }

        if (g_pci_drivers[i].type == PCI_DRIVER_CLASS && !rclass) {
            uint32_t match = g_pci_drivers[i].match;
            // Check if prog_if has to be matched
            // 0xFF means (match all prog. IF)
            if ((match & 0xFF) == 0xFF) {
                match &= ~0xFF;
                class_id &= ~0xFF;
            }

            if (match == class_id) {
                rclass = g_pci_drivers[i].init_func;
                continue;
            }
        } else if (g_pci_drivers[i].type == PCI_DRIVER_DEV && !rdev) {
            if (device_id == g_pci_drivers[i].match) {
                rdev = g_pci_drivers[i].init_func;
                continue;
            }
        }
    }

    *class_driver = rclass;
    *dev_driver = rdev;
}

static void pci_device_add(struct pci_device *dev) {
    dev->next = g_pci_devices;
    g_pci_devices = dev;
}

static int pcie_device_setup(struct pci_device *dev) {
    uint32_t class, caps_offset, irq_info;
    uint32_t id;
    uint8_t irq_pin;

    class = pcie_config_read_dword(dev, PCI_CONFIG_CLASS);
    caps_offset = pcie_config_read_dword(dev, PCI_CONFIG_CAPABILITIES) & 0xFF;
    irq_info = pcie_config_read_dword(dev, PCI_CONFIG_IRQ);
    id = pcie_config_read_dword(dev, PCI_CONFIG_ID);

    kinfo("%02x:%02x:%02x:\n", dev->bus, dev->dev, dev->func);
    kinfo(" Class %02x:%02x:%02x\n", (class >> 24), (class >> 16) & 0xFF, (class >> 8) & 0xFF);
    kinfo(" Device %04x:%04x\n", id & 0xFFFF, (id >> 16) & 0xFFFF);

    irq_pin = (irq_info >> 8) & 0xFF;
    if (irq_pin) {
        dev->irq_pin = irq_pin - 1;
        kinfo(" IRQ pin INT%c#\n", dev->irq_pin + 'A');
    }

    while (caps_offset) {
        uint8_t *link = (uint8_t *) (dev->pcie_config + caps_offset);

        switch (link[0]) {
        case 0x05:
            kinfo(" * MSI capability\n");
            dev->msi = (struct pci_cap_msi *) link;
            break;
        case 0x10:
            kinfo(" * PCIe capability\n");
            break;
        default:
            // Unknown capability
            kinfo(" * Device capability: %02x\n", link[0]);
            break;
        }

        caps_offset = link[1];
    }

    pci_driver_func_t driver_class, driver_dev;
    pci_pick_driver(id, class >> 8, &driver_class, &driver_dev);

    if (driver_dev) {
        driver_dev(dev);
        return 0;
    }

    if (driver_class) {
        driver_class(dev);
        return 0;
    }

    return -1;
}

static void pcie_enumerate_device(uintptr_t base_address, uint16_t seg, uint8_t start_bus, uint8_t bus, uint8_t dev_no) {
    void *cfg;
    uint32_t id;

    for (uint8_t func = 0; func < 8; ++func) {
        uint32_t d = ((uint32_t) (bus - start_bus) << 20) | ((uint32_t) dev_no << 15) | ((uint32_t) func << 12);
        cfg = (void *) MM_VIRTUALIZE(base_address + d);
        id = *(uint32_t *) cfg;
        if ((id & 0xFFFF) == 0xFFFF) {
            continue;
        }

        struct pci_device *dev = kmalloc(sizeof(struct pci_device));
        _assert(dev);
        dev->bus = bus;
        dev->dev = dev_no;
        dev->func = func;

        dev->pcie_segment_group = seg;
        dev->pcie_config = cfg;
        dev->msi = NULL;
        dev->irq_pin = -1;

        pcie_device_setup(dev);
        pci_device_add(dev);
    }
}

static void pcie_enumerate_bus(uintptr_t base_address, uint16_t seg, uint8_t start_bus, uint8_t bus) {
    for (uint8_t dev = 0; dev < 32; ++dev) {
        // Check if function 0 is present
        void *cfg = (void *) MM_VIRTUALIZE(base_address + (((bus - start_bus) << 20) | (dev << 15)));

        if (((*(uint32_t *) cfg) & 0xFFFF) != 0xFFFF) {
            uint32_t header = *(uint32_t *) (cfg + 0x0C);
            header >>= 16;
            header &= 0x7F;

            switch (header) {
            case 0x00:
                pcie_enumerate_device(base_address, seg, start_bus, bus, dev);
                break;
            default:
                kwarn("Skipping unsupported header type: %02x\n", header);
                break;
            }
        }
    }
}

static void pcie_enumerate_segment(uintptr_t base_address, uint16_t seg, uint8_t start_bus, uint8_t end_bus) {
    for (uint16_t bus = start_bus; bus < end_bus; ++bus) {
        pcie_enumerate_bus(base_address, seg, start_bus, bus);
    }
}

void pci_init(void) {
    if (!acpi_mcfg) {
        panic("TODO: legacy PCI\n");
    }

    uint32_t mcfg_entry_count = (acpi_mcfg->hdr.length - sizeof(struct acpi_header) - 8) / sizeof(struct acpi_mcfg_entry);
    kinfo("MCFG has %u entries:\n", mcfg_entry_count);
    for (uint32_t i = 0; i < mcfg_entry_count; ++i) {
        kinfo("%u:\n", i);
        kinfo("  Base address: %p\n", acpi_mcfg->entry[i].base_address);
        kinfo("  Segment group #%u\n", acpi_mcfg->entry[i].pci_segment_group);
        kinfo("  PCI buses: %02x-%02x\n", acpi_mcfg->entry[i].start_pci_bus, acpi_mcfg->entry[i].end_pci_bus);
    }

    // Start enumerating buses specified in MCFG
    for (uint32_t i = 0; i < mcfg_entry_count; ++i) {
        pcie_enumerate_segment(acpi_mcfg->entry[i].base_address,
                               acpi_mcfg->entry[i].pci_segment_group,
                               acpi_mcfg->entry[i].start_pci_bus,
                               acpi_mcfg->entry[i].end_pci_bus);
    }
}
