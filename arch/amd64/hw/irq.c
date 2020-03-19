#include "arch/amd64/asm/asm_irq.h"
#if defined(AMD64_MAX_SMP)
#include "arch/amd64/smp/ipi.h"
#endif
#include "arch/amd64/hw/ioapic.h"
#include "arch/amd64/hw/io.h"
#include "arch/amd64/hw/irq.h"
#include "arch/amd64/hw/idt.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"

#define IRQ_MAX             32      // Maximum assignable IRQ vectors
#define IRQ_MAX_HANDLERS    4       // Maximum handlers per IRQ vector (sharing)

#define MSI_BASE_VECTOR     0x80
#define MSI_MAX             4       // MSI vector count
#define MSI_MAX_HANDLERS    16      // Maximum handlers per MSI vector

static struct irq_handler handlers[IRQ_MAX * IRQ_MAX_HANDLERS] = {0};
static struct irq_handler msi_handlers[MSI_MAX_HANDLERS] = {0};
static uint64_t vector_bitmap[4] = {0};
static int ioapic_available = 0;

static int irq_alloc_vector(uint8_t *vector) {
    for (size_t i = 1; i < IRQ_MAX; ++i) {
        size_t idx = i / 64;
        size_t bit = 1ULL << (i % 64);

        if (!(vector_bitmap[idx] & bit)) {
            *vector = i;
            vector_bitmap[idx] |= bit;
            return 0;
        }
    }

    return -1;
}

void irq_handle(uintptr_t n) {
    _assert(n && n < IRQ_MAX);

    struct irq_handler *list = &handlers[IRQ_MAX_HANDLERS * n];
    for (size_t i = 0; i < IRQ_MAX_HANDLERS; ++i) {
        if (list[i].func && (list[i].func(list[i].ctx) == 0)) {
            return;
        }
    }
}

int irq_add_handler(uint8_t gsi, irq_handler_func_t handler, void *ctx) {
    _assert(gsi != 0 && gsi < IRQ_MAX);
    _assert(ioapic_available);

    uint8_t vector;
    if (irq_alloc_vector(&vector)) {
        return -1;
    }
    kdebug("I/O APIC: Mapping GSI%u -> Vector %d:%d\n", gsi, 0, vector);

    struct irq_handler *list = &handlers[IRQ_MAX_HANDLERS * vector];
    for (size_t i = 0; i < IRQ_MAX_HANDLERS; ++i) {
        if (!list[i].func) {
            list[i].func = handler;
            list[i].ctx = ctx;

            amd64_ioapic_map_gsi(gsi, 0, vector + 0x20);
            amd64_ioapic_unmask(gsi);

            return 0;
        }
    }

    return -1;
}

int irq_add_leg_handler(uint8_t leg_irq, irq_handler_func_t handler, void *ctx) {
    _assert(leg_irq != 0 && leg_irq < IRQ_MAX);

    if (ioapic_available) {
        // Find out the route (TODO)
        uint8_t gsi = amd64_ioapic_leg_gsi(leg_irq);
        return irq_add_handler(gsi, handler, ctx);
    } else {
        struct irq_handler *list = &handlers[IRQ_MAX_HANDLERS * leg_irq];

        for (size_t i = 0; i < IRQ_MAX_HANDLERS; ++i) {
            if (!list[i].func) {
                list[i].func = handler;
                list[i].ctx = ctx;
                return 0;
            }
        }

        return -1;
    }
}

int irq_add_pci_handler(struct pci_device *dev, uint8_t pin, irq_handler_func_t handler, void *ctx) {
    _assert(pin < 4);
    uint32_t irq_route = amd64_pci_pin_irq_route(dev, pin);
    _assert(irq_route != PCI_IRQ_INVALID);

    if (irq_route == PCI_IRQ_NO_ROUTE) {
        panic("TODO: allocate PCI IRQ routes\n");
    }

    //kdebug("Assigning handler to " PCI_FMTADDR " INT%c# -> GSI%d", PCI_VAADDR(addr), pin + 'A', irq_route);

    return irq_add_handler(irq_route, handler, ctx);
}

void irq_enable_ioapic_mode(void) {
    ioapic_available = 1;
    // Copy current table
    struct irq_handler handlers_old[16 * IRQ_MAX_HANDLERS];

    memcpy(handlers_old, handlers, 16 * IRQ_MAX_HANDLERS * sizeof(struct irq_handler));
    memset(handlers, 0, 16 * IRQ_MAX_HANDLERS * sizeof(struct irq_handler));

    // All legacy IRQs
    for (size_t i = 0; i < 16; ++i) {
        struct irq_handler *handler_list = &handlers_old[i * IRQ_MAX_HANDLERS];

        for (size_t j = 0; j < IRQ_MAX_HANDLERS; ++j) {
            if (handler_list[j].func) {
                irq_add_leg_handler(i,
                    handlers_old[i * IRQ_MAX_HANDLERS + j].func,
                    handlers_old[i * IRQ_MAX_HANDLERS + j].ctx);
            }
        }
    }
}

int irq_has_handler(uint8_t gsi) {
    _assert(gsi < IRQ_MAX);
    _assert(ioapic_available);

    struct irq_handler *handler_list = &handlers[gsi * IRQ_MAX_HANDLERS];
    return !!handler_list[0].func;
}

int irq_add_msi_handler(irq_handler_func_t handler, void *ctx, uint8_t *vector) {
    for (size_t i = 0; i < MSI_MAX_HANDLERS; ++i) {
        if (!msi_handlers[i].func) {
            msi_handlers[i].func = handler;
            msi_handlers[i].ctx = ctx;

            *vector = 0x80;
            return 0;
        }
    }
    return -1;
}

void amd64_msi_handle(uint64_t vector) {
    for (size_t i = 0; i < MSI_MAX_HANDLERS; ++i) {
        if (!msi_handlers[i].func) {
            break;
        }

        if (msi_handlers[i].func(msi_handlers[i].ctx) == IRQ_HANDLED) {
            return;
        }
    }
    kwarn("Unhandled MSI on vector %u\n", vector);
}

void irq_init(int cpu) {
    // Special entry
    amd64_idt_set(cpu, 32, (uintptr_t) amd64_irq0_early, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    amd64_idt_set(cpu, 33, (uintptr_t) amd64_irq1, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 34, (uintptr_t) amd64_irq2, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 35, (uintptr_t) amd64_irq3, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 36, (uintptr_t) amd64_irq4, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 37, (uintptr_t) amd64_irq5, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 38, (uintptr_t) amd64_irq6, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 39, (uintptr_t) amd64_irq7, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 40, (uintptr_t) amd64_irq8, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 41, (uintptr_t) amd64_irq9, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 42, (uintptr_t) amd64_irq10, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 43, (uintptr_t) amd64_irq11, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 44, (uintptr_t) amd64_irq12, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 45, (uintptr_t) amd64_irq13, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 46, (uintptr_t) amd64_irq14, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, 47, (uintptr_t) amd64_irq15, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    // No load balancing for MSIs yet
    if (cpu == 0) {
        // Message signaled interrupt support
        // Message address register format:
        //  0 ..  1         Unused
        //  2               Destination mode
        //  3               Redirection hint
        //  4 .. 11         Reserved
        // 12 .. 19         Destination APIC ID
        // 31 .. 20         0xFEE
        //
        // DM RH            Behavior
        //  0  0            Destination ID field specifies target CPU
        //  1  0            Destination ID must point to a valid cpu
        //  0  1            Only matching CPU receives interrupt (without redirection)
        //  1  1            Redirection is limited to only the Destination ID's logical group
        //
        // Message data register format:
        //  0 ..  7         Interrupt vector (0x80)
        //  8 .. 10         Delivery mode
        // 11 .. 13         Reserved
        // 14               Trigger level (don't care, all are edge-trigggered)
        // 15               Level/Edge trigger
        // 16 .. 63         Reserved
        amd64_idt_set(cpu, MSI_BASE_VECTOR + 0x00, (uintptr_t) amd64_irq_msi0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    }

#if defined(AMD64_SMP)
    // Common for all CPUs
    amd64_idt_set(cpu, IPI_VECTOR_GENERIC, (uintptr_t) amd64_irq_ipi, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(cpu, IPI_VECTOR_PANIC, (uintptr_t) amd64_irq_ipi_panic, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
#endif
}
