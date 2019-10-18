#include "sys/amd64/hw/ioapic.h"
#include "sys/string.h"
#include "sys/mm.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/assert.h"
#include "sys/amd64/hw/pci/pci.h"
#include "acpi.h"

uintptr_t amd64_ioapic_base = 0;

// Legacy PIC -> I/O APIC routing table
static uint32_t ioapic_leg_rt[16] = { 0xFFFFFFFF };

uint32_t amd64_ioapic_read(uint8_t reg) {
    if (!amd64_ioapic_base) {
        // TODO: panic
        kdebug("Reading non-existent I/O APIC\n");
        while (1);
    }
    uint32_t volatile *ioapic = (uint32_t volatile *) amd64_ioapic_base;
    ioapic[0] = (reg & 0xFF);
    return ioapic[4];
}

void amd64_ioapic_write(uint8_t reg, uint32_t v) {
    if (!amd64_ioapic_base) {
        // TODO: panic
        kdebug("Writing non-existent I/O APIC\n");
        while (1);
    }
    uint32_t volatile *ioapic = (uint32_t volatile *) amd64_ioapic_base;
    ioapic[0] = reg & 0xFF;
    ioapic[4] = v;
}

void amd64_ioapic_int_src_override(uint8_t bus_src, uint8_t irq_src, uint32_t no, uint16_t flags) {
    kdebug("IRQ Override: %02x:%02x -> %d\n", bus_src, irq_src, no);
    if (bus_src == 0 && irq_src < 16) {
        // Totally a legacy IRQ
        ioapic_leg_rt[irq_src] = no;
    }

    // Setup flags properly
    uint32_t low = amd64_ioapic_read(0x10 + no * 2);

    if (flags & (1 << 1)) {
        // Low-active IRQ
        low |= IOAPIC_REDIR_POL_LOW;
    } else {
        low &= ~IOAPIC_REDIR_POL_LOW;
    }

    if (flags & (1 << 3)) {
        low |= IOAPIC_REDIR_TRG_LEVEL;
    } else {
        low &= ~IOAPIC_REDIR_TRG_LEVEL;
    }

    amd64_ioapic_write(0x10 + no * 2, low);
}

void amd64_ioapic_set(uintptr_t addr) {
    if (amd64_ioapic_base != 0) {
        // TODO: panic
        kdebug("I/O APIC base already set\n");
        while (1);
    }
    amd64_ioapic_base = addr;

//    kdebug("I/O APIC @ %p\n", addr);

    // Dump the entries of I/O APIC

    for (uint8_t i = 0x10; i < 0x3F; i += 2) {
        uint32_t low = amd64_ioapic_read(i + 0);
        uint32_t high = amd64_ioapic_read(i + 1);

        static const char *del_type[] = {
            "Normal",
            "Low priority",
            "SMI",
            "???",
            "NMI",
            "INIT",
            "???",
            "EXTI"
        };

//        kdebug("[%d] %s, %s, DST 0x%02x:0x%02x\n",
//               i,
//               (low & IOAPIC_REDIR_MSK) ? "Masked" : "",
//               del_type[(low >> 8) & 0x7],
//               IOAPIC_REDIR_DST_GET(high),
//               low & 0xF);
    }

    // Test: set IRQ1 -> LAPIC 0:1 and unmask it
    uint32_t low = amd64_ioapic_read(0x10 + 1 * 2);
    uint32_t high = amd64_ioapic_read(0x11 + 1 * 2);

    low &= ~0x7;
    low &= ~IOAPIC_REDIR_MSK;

    low |= 0x1 + 0x20;
    high = IOAPIC_REDIR_DST_SET(0);

    amd64_ioapic_write(0x10 + 1 * 2, low);
    amd64_ioapic_write(0x11 + 1 * 2, high);

    // TODO: unconfigured PCI IRQs may be mapped here
}

#define PCI_LINK_MAX_POSSIBLE_IRQS      16
#define PCI_MAX_IRQ_ROUTES              128
#define PCI_MAX_LINKS                   32

static struct acpi_pci_link {
    // Fully-qualified path name
    char acpi_name[32];
    uint32_t possible_count;
    uint32_t possible_interrupts[PCI_LINK_MAX_POSSIBLE_IRQS];
    uint32_t current_interrupt;
} pci_links[PCI_MAX_LINKS];

static struct acpi_irq_route {
    uint8_t bus;
    uint8_t dev;
    uint8_t pin;

    int is_named;
    union {
        char dst_name[32];
        uint32_t dst_interrupt;
    };
} pci_irq_routing[PCI_MAX_IRQ_ROUTES];
static size_t acpi_pci_irq_routes = 0;

static struct acpi_pci_link *acpi_pci_link_allocate(void) {
    for (size_t i = 0; i < PCI_MAX_LINKS; ++i) {
        if (!pci_links[i].acpi_name[0]) {
            return &pci_links[i];
        }
    }
    return NULL;
}

static int acpi_pci_link_add_possible_interrupt(struct acpi_pci_link *link, uint32_t irq) {
    if (link->possible_count == PCI_LINK_MAX_POSSIBLE_IRQS) {
        return -1;
    }

    link->possible_interrupts[link->possible_count++] = irq;
    return 0;
}

static int acpi_pci_add_named_route(uint8_t bus, uint8_t dev, uint8_t pin, const char *link_name) {
    if (acpi_pci_irq_routes == PCI_MAX_IRQ_ROUTES) {
        return -1;
    }
    _assert(strlen(link_name) < 32);
    struct acpi_irq_route *route = &pci_irq_routing[acpi_pci_irq_routes++];

    strcpy(route->dst_name, link_name);
    route->is_named = 1;
    route->bus = bus;
    route->dev = dev;
    route->pin = pin;

    return 0;
}

static int acpi_pci_add_hard_route(uint8_t bus, uint8_t dev, uint8_t pin, uint32_t irq) {
    if (acpi_pci_irq_routes == PCI_MAX_IRQ_ROUTES) {
        return -1;
    }
    struct acpi_irq_route *route = &pci_irq_routing[acpi_pci_irq_routes++];

    route->dst_interrupt = irq;
    route->is_named = 0;
    route->bus = bus;
    route->dev = dev;
    route->pin = pin;

    return 0;
}

static ACPI_STATUS acpi_dev_walk_pci_link_possible_resources(ACPI_RESOURCE *Resource, void *Ctx) {
    struct acpi_pci_link *link = Ctx;

    switch (Resource->Type) {
    case ACPI_RESOURCE_TYPE_IRQ: {
            ACPI_RESOURCE_IRQ *Irq = &Resource->Data.Irq;

            for (UINT32 i = 0; i < Irq->InterruptCount; ++i) {
                _assert(acpi_pci_link_add_possible_interrupt(link, Irq->Interrupts[i]) == 0);
            }
        }
        return AE_OK;
    case ACPI_RESOURCE_TYPE_EXTENDED_IRQ: {
            ACPI_RESOURCE_EXTENDED_IRQ *ExtendedIrq = &Resource->Data.ExtendedIrq;

            for (UINT32 i = 0; i < ExtendedIrq->InterruptCount; ++i) {
                _assert(acpi_pci_link_add_possible_interrupt(link, ExtendedIrq->Interrupts[i]) == 0);
            }
        }
        return AE_OK;
    case ACPI_RESOURCE_TYPE_END_TAG:
        return AE_OK;
    default:
        panic("Unknown resource type for link: %02x\n", Resource->Type);
    }
}

static ACPI_STATUS acpi_dev_walk_pci_link_current_resources(ACPI_RESOURCE *Resource, void *Ctx) {
    struct acpi_pci_link *link = Ctx;

    switch (Resource->Type) {
    case ACPI_RESOURCE_TYPE_IRQ: {
            ACPI_RESOURCE_IRQ *Irq = &Resource->Data.Irq;
            if (Irq->InterruptCount == 0) {
                // Leave in "Unconfigured" state
                return AE_OK;
            }
            assert(Irq->InterruptCount == 1, "Only one current IRQ config supported now\n");

            link->current_interrupt = Irq->Interrupts[0];
        }
        return AE_OK;
    case ACPI_RESOURCE_TYPE_EXTENDED_IRQ: {
            ACPI_RESOURCE_EXTENDED_IRQ *ExtendedIrq = &Resource->Data.ExtendedIrq;
            if (ExtendedIrq->InterruptCount == 0) {
                // Leave in "Unconfigured" state
                return AE_OK;
            }
            assert(ExtendedIrq->InterruptCount == 1, "Only one current IRQ config supported now\n");

            link->current_interrupt = ExtendedIrq->Interrupts[0];
        }
        return AE_OK;
    case ACPI_RESOURCE_TYPE_END_TAG:
        return AE_OK;
    default:
        panic("Unknown resource type for link: %02x\n", Resource->Type);
    }
}

static ACPI_STATUS acpi_dev_walk_pci_link(ACPI_HANDLE LinkDevice, UINT32 NestingLevel, void *Ctx, void **Res) {
    ACPI_STATUS ret;
    char ResourceBufferData[256];
    char LinkName[256];
    ACPI_BUFFER ResourceBuffer = { sizeof(ResourceBufferData), ResourceBufferData };
    ACPI_BUFFER LinkNameBuffer = { sizeof(LinkName), LinkName };

    if (ACPI_FAILURE(ret = AcpiGetName(LinkDevice, ACPI_FULL_PATHNAME, &LinkNameBuffer))) {
        return ret;
    }

    struct acpi_pci_link *link = acpi_pci_link_allocate();
    _assert(link);
    _assert(strlen(LinkName) < 32);
    strcpy(link->acpi_name, LinkName);
    link->possible_count = 0;
    link->current_interrupt = PCI_IRQ_NO_ROUTE;

    if (ACPI_FAILURE(ret = AcpiGetPossibleResources(LinkDevice, &ResourceBuffer))) {
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiWalkResourceBuffer(&ResourceBuffer, acpi_dev_walk_pci_link_possible_resources, link))) {
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiGetCurrentResources(LinkDevice, &ResourceBuffer))) {
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiWalkResourceBuffer(&ResourceBuffer, acpi_dev_walk_pci_link_current_resources, link))) {
        return ret;
    }

    return AE_OK;
}

static ACPI_STATUS acpi_dev_walk_pci_buses(ACPI_HANDLE BusDevice, UINT32 NestingLevel, void *Ctx, void **Res) {
    ACPI_STATUS ret;

    char BusName[256];
    ACPI_OBJECT BusNumberObject;
    ACPI_PCI_ROUTING_TABLE IrqRoutingTable[256];
    ACPI_DEVICE_INFO *DeviceInfo = NULL;

    ACPI_BUFFER BusNameBuffer = { sizeof(BusName), BusName };
    ACPI_BUFFER BusNumberObjectBuffer = { sizeof(BusNumberObject), &BusNumberObject };
    ACPI_BUFFER IrqRoutingTableBuffer = { sizeof(IrqRoutingTable), IrqRoutingTable };

    if (ACPI_FAILURE(ret = AcpiGetName(BusDevice, ACPI_FULL_PATHNAME, &BusNameBuffer))) {
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiGetObjectInfo(BusDevice, &DeviceInfo))) {
        return ret;
    }

    UINT8 Bus = 0;
    int assigned_bus_number = 0;

    // Try to get bus number from _BBN (Base Bus Number)
    if (ACPI_FAILURE(ret = AcpiEvaluateObjectTyped(BusDevice, "_BBN", NULL, &BusNumberObjectBuffer, ACPI_TYPE_INTEGER))) {
        if (ret != AE_NOT_FOUND) {
            return ret;
        }
    } else {
        assigned_bus_number = 1;
        kdebug("%s._BBN = %p\n", BusName, BusNumberObject.Integer.Value);
        if (BusNumberObject.Integer.Value > 0xFF) {
            panic("Bus number is too high: %p\n", BusNumberObject.Integer.Value);
        }
        Bus = BusNumberObject.Integer.Value;
    }

    kdebug("PCI Root Bus: 0x%02x (%s)\n", Bus, BusName);
    kdebug("  _HID = %s, _CLS = %s\n",
        ((DeviceInfo->Valid & ACPI_VALID_HID) ? DeviceInfo->HardwareId.String : "NONE"),
        ((DeviceInfo->Valid & ACPI_VALID_CLS) ? DeviceInfo->ClassCode.String : "NONE"));
    if (DeviceInfo->Valid & ACPI_VALID_CID) {
        for (size_t i = 0; i < DeviceInfo->CompatibleIdList.Count; ++i) {
            kdebug("  _CID = %s\n", DeviceInfo->CompatibleIdList.Ids[i].String);
        }
    }

    pci_add_root_bus(Bus);

    // Get _PRT
    if (ACPI_FAILURE(ret = AcpiGetIrqRoutingTable(BusDevice, &IrqRoutingTableBuffer))) {
        return ret;
    }

    UINT32 Offset = 0;

    while (Offset < IrqRoutingTableBuffer.Length) {
        ACPI_PCI_ROUTING_TABLE *Route = (ACPI_PCI_ROUTING_TABLE *) ((uintptr_t) IrqRoutingTable + Offset);

        if (!Route->Length) {
            break;
        }

        UINT8 Device = (Route->Address >> 16) & 0xFF;
        _assert((Route->Address & 0xFFFF) == 0xFFFF);

        // Check if it's a "hard" route, meaning it's not a reference to a PCI Link device but
        // GSI number is specified directly instead
        if (Route->SourceIndex == 0) {
            // It's a LNKx reference
            _assert(acpi_pci_add_named_route(Bus, Device, Route->Pin, Route->Source) == 0);
        } else {
            // IRQ number specified directly
            _assert(acpi_pci_add_hard_route(Bus, Device, Route->Pin, Route->SourceIndex) == 0);
        }

        Offset += Route->Length;
    }

    return AE_OK;
}

int amd64_pci_init_irqs(void) {
    ACPI_STATUS ret;

    // 1. Walk links
    if (ACPI_FAILURE(ret = AcpiGetDevices("PNP0C0F", acpi_dev_walk_pci_link, NULL, NULL))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return -1;
    }

    // 2. Walk buses
    // Will also match PCI Express buses
    if (ACPI_FAILURE(ret = AcpiGetDevices("PNP0A03" /* PCI Bus */, acpi_dev_walk_pci_buses, NULL, NULL))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return -1;
    }

    // Dump PCI Link table
    kdebug("PCI Interrupt Link Devices:\n");
    for (size_t i = 0; i < 8; ++i) {
        if (pci_links[i].acpi_name[0]) {
            if (pci_links[i].current_interrupt == PCI_IRQ_NO_ROUTE) {
                kdebug("%s: Unmapped\n", pci_links[i].acpi_name);
            } else {
                kdebug("%s: Current IRQ %u\n", pci_links[i].acpi_name, pci_links[i].current_interrupt);
            }
        }
    }

    return 0;
}

uint32_t amd64_pci_link_route(const char *link_name) {
    for (size_t i = 0; i < PCI_MAX_LINKS; ++i) {
        if (!strcmp(link_name, pci_links[i].acpi_name)) {
            return pci_links[i].current_interrupt;
        }
    }

    // No such link exists
    kdebug("No link named %s\n", link_name);
    return PCI_IRQ_INVALID;
}

uint32_t amd64_pci_pin_irq_route(uint8_t bus, uint8_t dev, uint8_t func, uint8_t pin) {
    for (size_t i = 0; i < acpi_pci_irq_routes; ++i) {
        struct acpi_irq_route *route = &pci_irq_routing[i];

        if (route->bus == bus && route->dev == dev && route->pin == pin) {
            // Found matching route
            if (route->is_named) {
                // Resolve LNKx
                return amd64_pci_link_route(route->dst_name);
            } else {
                return route->dst_interrupt;
            }
        }
    }

    // No such route exists
    kerror("No PCI IRQ route %02x:%02x:%02x:INT%c#\n", bus, dev, func, pin + 'A');
    return PCI_IRQ_INVALID;
}
