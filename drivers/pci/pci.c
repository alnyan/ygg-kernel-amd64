#include "arch/amd64/hw/ioapic.h"
#include "arch/amd64/hw/ioapic.h"
#include "arch/amd64/hw/acpi.h"
#include "arch/amd64/hw/io.h"
#include "drivers/pci/pci.h"
#include "sys/snprintf.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "fs/sysfs.h"
#include "sys/heap.h"
#include "sys/mm.h"

struct pci_device;
struct pci_driver;

#define PCI_MAX_DRIVERS         64
#define PCI_IS_EXPRESS(dev)     ((dev)->pcie_segment_group != (uint16_t) -1)

#define pcie_config_read_dword(dev, off) \
    (*(uint32_t *) ((dev)->pcie_config + (off)))

#define pcie_config_write_dword(dev, off, val) \
    (*(uint32_t *) ((dev)->pcie_config + (off))) = (val);

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

    struct pci_driver *driver;

    // For list
    struct pci_device *next;
};

struct pci_driver {
    char name[64];
    pci_driver_func_t init_func;
    uint32_t type;
    uint32_t match;
};

static struct pci_device *g_pci_devices = NULL;
static struct pci_driver  g_pci_drivers[PCI_MAX_DRIVERS] = {0};
static size_t             g_pci_driver_count;

#define PCI_DRIVER_CLASS        1
#define PCI_DRIVER_DEV          2

void pci_config_write_dword(struct pci_device *dev, uint16_t off, uint32_t val) {
    if (PCI_IS_EXPRESS(dev)) {
        pcie_config_write_dword(dev, off, val);
    } else {
        pci_config_write_dword_legacy(dev->bus, dev->dev, dev->func, off, val);
    }
}

void pci_config_write_dword_legacy(uint8_t bus, uint8_t dev, uint8_t func, uint32_t off, uint32_t val) {
    uint32_t w0;
    w0 = (((uint32_t) bus) << 16) |
         (((uint32_t) dev) << 11) |
         (((uint32_t) func) << 8) |
         (off & ~0x3) |
         (1 << 31);

    outl(PCI_PORT_CONFIG_ADDR, w0);
    outl(PCI_PORT_CONFIG_DATA, val);
}

uint32_t pci_config_read_dword(struct pci_device *dev, uint16_t off) {
    if (PCI_IS_EXPRESS(dev)) {
        return pcie_config_read_dword(dev, off);
    } else {
        return pci_config_read_dword_legacy(dev->bus, dev->dev, dev->func, off);
    }
}

uint32_t pci_config_read_dword_legacy(uint8_t bus, uint8_t dev, uint8_t func, uint32_t off) {
    uint32_t w0;
    w0 = (((uint32_t) bus) << 16) |
         (((uint32_t) dev) << 11) |
         (((uint32_t) func) << 8) |
         (off & ~0x3) |
         (1U << 31);

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
    } else {
        uint32_t irq_config = pci_config_read_dword(dev, PCI_CONFIG_IRQ);
        uint8_t irq_pin = (irq_config >> 8) & 0xFF;
        if (irq_pin) {
            kdebug("Uses INT%c# IRQ pin\n\n", 'A' + irq_pin - 1);
            irq_add_pci_handler(dev, irq_pin - 1, handler, ctx);
        }
    }
}

void pci_add_class_driver(uint32_t full_class, pci_driver_func_t func, const char *name) {
    if (g_pci_driver_count == PCI_MAX_DRIVERS) {
        panic("Too many PCI drivers loaded\n");
    }

    struct pci_driver *driver = &g_pci_drivers[g_pci_driver_count++];

    strcpy(driver->name, name);
    driver->init_func = func;
    driver->type = PCI_DRIVER_CLASS;
    driver->match = full_class & ~0xFF000000;
}

void pci_add_device_driver(uint32_t id, pci_driver_func_t func, const char *name) {
    if (g_pci_driver_count == PCI_MAX_DRIVERS) {
        panic("Too many PCI drivers loaded\n");
    }

    struct pci_driver *driver = &g_pci_drivers[g_pci_driver_count++];

    strcpy(driver->name, name);
    driver->init_func = func;
    driver->type = PCI_DRIVER_DEV;
    driver->match = id;
}

void pci_add_root_bus(uint8_t n) {
    kwarn("%s: %02x\n", __func__, n);
}

void pci_get_device_address(const struct pci_device *dev, uint8_t *b, uint8_t *d, uint8_t *f) {
    *b = dev->bus;
    *d = dev->dev;
    *f = dev->func;
}

static void pci_pick_driver(uint32_t device_id, uint32_t class_id, struct pci_driver **class_driver, struct pci_driver **dev_driver) {
    struct pci_driver *rclass = NULL, *rdev = NULL;
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
                rclass = &g_pci_drivers[i];
                continue;
            }
        } else if (g_pci_drivers[i].type == PCI_DRIVER_DEV && !rdev) {
            if (device_id == g_pci_drivers[i].match) {
                rdev = &g_pci_drivers[i];
                continue;
            }
        }
    }

    *class_driver = rclass;
    *dev_driver = rdev;
}

static int sysfs_pci_device_read(void *ctx, char *buf, size_t lim) {
    struct pci_device *dev = ctx;
    _assert(dev);
    uint32_t id = pci_config_read_dword(dev, PCI_CONFIG_ID);
    uint32_t class = pci_config_read_dword(dev, PCI_CONFIG_CLASS);
    uint32_t bar[6];
    for (size_t i = 0; i < 6; ++i) {
        bar[i] = pci_config_read_dword(dev, PCI_CONFIG_BAR(i));
    }

    sysfs_buf_printf(buf, lim, "vendor      %04x\n", id & 0xFFFF);
    sysfs_buf_printf(buf, lim, "device      %04x\n", id >> 16);

    sysfs_buf_printf(buf, lim, "class       %02x\n", class >> 24);
    sysfs_buf_printf(buf, lim, "subclass    %02x\n", (class >> 16) & 0xFF);
    sysfs_buf_printf(buf, lim, "prog if     %02x\n", (class >> 8) & 0xFF);

    for (size_t i = 0; i < 6; ++i) {
        sysfs_buf_printf(buf, lim, "bar%d        %08x\n", i, bar[i]);
    }

    if (dev->driver) {
        sysfs_buf_printf(buf, lim, "driver      %s\n", dev->driver->name);
    }

    return 0;
}

static void pci_device_add(struct pci_device *dev) {
    // For testing purposes
    char name[256];
    snprintf(name, sizeof(name), "pci.%02x.%02x.%02x", dev->bus, dev->dev, dev->func);
    sysfs_add_config_endpoint(name, SYSFS_MODE_DEFAULT, 512, dev, sysfs_pci_device_read, NULL);

    dev->next = g_pci_devices;
    g_pci_devices = dev;
}

static int pci_device_setup(struct pci_device *dev) {
    uint32_t class, caps_offset, irq_info;
    uint32_t id;
    uint8_t irq_pin;

    dev->driver = NULL;

    class = pci_config_read_dword(dev, PCI_CONFIG_CLASS);
    caps_offset = pci_config_read_dword(dev, PCI_CONFIG_CAPABILITIES) & 0xFF;
    irq_info = pci_config_read_dword(dev, PCI_CONFIG_IRQ);
    id = pci_config_read_dword(dev, PCI_CONFIG_ID);

    kdebug("%02x:%02x:%02x:\n", dev->bus, dev->dev, dev->func);
    kdebug(" Class %02x:%02x:%02x\n", (class >> 24), (class >> 16) & 0xFF, (class >> 8) & 0xFF);
    kdebug(" Device %04x:%04x\n", id & 0xFFFF, (id >> 16) & 0xFFFF);

    irq_pin = (irq_info >> 8) & 0xFF;
    if (irq_pin) {
        dev->irq_pin = irq_pin - 1;
        kdebug(" IRQ pin INT%c#\n", dev->irq_pin + 'A');
    }

    if (PCI_IS_EXPRESS(dev)) {
        while (caps_offset) {
            uint8_t *link = (uint8_t *) (dev->pcie_config + caps_offset);

            switch (link[0]) {
            case 0x05:
                kdebug(" * MSI capability\n");
                dev->msi = (struct pci_cap_msi *) link;
                break;
            case 0x10:
                kdebug(" * PCIe capability\n");
                break;
            default:
                // Unknown capability
                kdebug(" * Device capability: %02x\n", link[0]);
                break;
            }

            caps_offset = link[1];
        }
    } else if (caps_offset) {
        kdebug("Skipping device capabilities: unsupported yet in legacy mode\n");
    }

    struct pci_driver *driver_class, *driver_dev;
    pci_pick_driver(id, class >> 8, &driver_class, &driver_dev);

    if (driver_dev) {
        dev->driver = driver_dev;
        driver_dev->init_func(dev);
        return 0;
    }

    if (driver_class) {
        dev->driver = driver_class;
        driver_class->init_func(dev);
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

        pci_device_setup(dev);
        pci_device_add(dev);
    }
}

static void pci_enumerate_device(uint8_t bus, uint8_t dev_no) {
    uint32_t id;

    for (uint8_t func = 0; func < 8; ++func) {
        id = pci_config_read_dword_legacy(bus, dev_no, func, PCI_CONFIG_ID);

        if ((id & 0xFFFF) == 0xFFFF) {
            continue;
        }

        struct pci_device *dev = kmalloc(sizeof(struct pci_device));
        _assert(dev);

        dev->bus = bus;
        dev->dev = dev_no;
        dev->func = func;

        dev->pcie_segment_group = (uint16_t) -1;
        dev->msi = NULL;
        dev->irq_pin = -1;

        pci_device_setup(dev);
        pci_device_add(dev);
    }
}

static void pci_enumerate_bus(uint8_t bus) {
    for (uint8_t dev = 0; dev < 32; ++dev) {
        uint32_t id = pci_config_read_dword_legacy(bus, 0, 0, PCI_CONFIG_ID);

        if ((id & 0xFFFF) != 0xFFFF) {
            uint32_t header = pci_config_read_dword_legacy(bus, 0, 0, 0x0C);
            header >>= 16;
            header &= 0x7F;

            switch (header) {
            case 0x00:
                pci_enumerate_device(bus, dev);
                break;
            default:
                kwarn("Skipping unsupported header type: %02x\n", header);
                break;
            }
        }
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
    amd64_pci_init_irqs();

    if (acpi_mcfg) {
        uint32_t mcfg_entry_count = (acpi_mcfg->hdr.length - sizeof(struct acpi_header) - 8) / sizeof(struct acpi_mcfg_entry);
        kdebug("MCFG has %u entries:\n", mcfg_entry_count);
        for (uint32_t i = 0; i < mcfg_entry_count; ++i) {
            kdebug("%u:\n", i);
            kdebug("  Base address: %p\n", acpi_mcfg->entry[i].base_address);
            kdebug("  Segment group #%u\n", acpi_mcfg->entry[i].pci_segment_group);
            kdebug("  PCI buses: %02x-%02x\n", acpi_mcfg->entry[i].start_pci_bus, acpi_mcfg->entry[i].end_pci_bus);
        }

        // Start enumerating buses specified in MCFG
        for (uint32_t i = 0; i < mcfg_entry_count; ++i) {
            pcie_enumerate_segment(acpi_mcfg->entry[i].base_address,
                                   acpi_mcfg->entry[i].pci_segment_group,
                                   acpi_mcfg->entry[i].start_pci_bus,
                                   acpi_mcfg->entry[i].end_pci_bus);
        }
    } else {
        kinfo("Using legacy PCI mechanism\n");
        for (uint8_t bus = 0; bus < 16; ++bus) {
            pci_enumerate_bus(bus);
        }
    }
}
