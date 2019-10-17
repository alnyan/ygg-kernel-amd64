#include "sys/debug.h"
#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/mm/mm.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/pci/pci.h"
#include "sys/panic.h"
#include "sys/assert.h"

#include "acpi.h"

static multiboot_info_t *multiboot_info;

#define PCI_LINK_MAX_POSSIBLE_IRQS      16
static struct acpi_pci_link {
    // Fully-qualified path name
    char acpi_name[256];
    uint32_t possible_count;
    uint32_t possible_interrupts[PCI_LINK_MAX_POSSIBLE_IRQS];
    uint32_t current_interrupt;
} pci_links[8];

static struct acpi_pci_link *acpi_pci_link_allocate(void) {
    for (size_t i = 0; i < 8; ++i) {
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
    strcpy(link->acpi_name, LinkName);
    link->possible_count = 0;
    link->current_interrupt = (UINT32) -1;

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

static ACPI_STATUS acpica_init(void) {
    ACPI_STATUS ret;
    ACPI_OBJECT arg1;
    ACPI_OBJECT_LIST args;

    /*
    * 0 = PIC
    * 1 = APIC
    * 2 = SAPIC ?
    */
    arg1.Type = ACPI_TYPE_INTEGER;
    arg1.Integer.Value = 1;
    args.Count = 1;
    args.Pointer = &arg1;

    if (ACPI_FAILURE(ret = AcpiInitializeSubsystem())) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInitializeTables(NULL, 0, FALSE))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiLoadTables())) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiEvaluateObject(NULL, "\\_PIC", &args, NULL))) {
        if (ret != AE_NOT_FOUND) {
            kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
            return ret;
        }
        kwarn("\\_PIC = AE_NOT_FOUND\n");
        // Guess that's ok?
    }

    if (ACPI_FAILURE(ret = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    // 1. Walk links
    if (ACPI_FAILURE(ret = AcpiGetDevices("PNP0C0F", acpi_dev_walk_pci_link, NULL, NULL))) {
        kerror("ACPI INIT failure %s\n", AcpiFormatException(ret));
        return ret;
    }

    return AE_OK;
}

void kernel_main(struct amd64_loader_data *data) {
    data = (struct amd64_loader_data *) MM_VIRTUALIZE(data);
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(data->multiboot_info_ptr);

    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr),
                          multiboot_info->mmap_length);

    amd64_gdt_init();
    amd64_idt_init();
    amd64_mm_init(data);

    if (ACPI_FAILURE(acpica_init())) {
        panic("Failed to initialize ACPICA\n");
    }

    // Dump PCI Link table
    kdebug("PCI Interrupt Link Devices:\n");
    for (size_t i = 0; i < 8; ++i) {
        if (pci_links[i].acpi_name[0]) {
            kdebug("%s: Current IRQ %u\n", pci_links[i].acpi_name, pci_links[i].current_interrupt);
        }
    }

    // TODO: if there are unconfigured IRQs, it's time to allocate GSIs and map them

    pci_enumerate();

    extern void sched_init(void);
    sched_init();
    amd64_acpi_init();

    amd64_syscall_init();

    while (1) {
        asm ("sti; hlt");
    }
}
