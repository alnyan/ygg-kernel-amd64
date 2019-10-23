#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/hw/ioapic.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/idt.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"

#define IRQ_MAX             32      // Maximum assignable IRQ vectors
#define IRQ_MAX_HANDLERS    4       // Maximum handlers per IRQ vector (sharing)

static irq_handler_t handlers[IRQ_MAX * IRQ_MAX_HANDLERS] = {0};
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

    irq_handler_t *list = &handlers[IRQ_MAX_HANDLERS * n];
    for (size_t i = 0; i < IRQ_MAX_HANDLERS; ++i) {
        if (list[i] && (list[i]() == 0)) {
            return;
        }
    }
}

int irq_add_handler(uint8_t gsi, irq_handler_t handler) {
    _assert(gsi != 0 && gsi < IRQ_MAX);
    _assert(ioapic_available);


    uint8_t vector;
    if (irq_alloc_vector(&vector)) {
        return -1;
    }
    kdebug("I/O APIC: Mapping GSI%u -> Vector %d:%d\n", gsi, 0, vector);

    irq_handler_t *list = &handlers[IRQ_MAX_HANDLERS * vector];
    for (size_t i = 0; i < IRQ_MAX_HANDLERS; ++i) {
        if (!list[i]) {
            list[i] = handler;

            amd64_ioapic_map_gsi(gsi, 0, vector + 0x20);
            amd64_ioapic_unmask(gsi);

            return 0;
        }
    }

    return -1;
}

int irq_add_leg_handler(uint8_t leg_irq, irq_handler_t handler) {
    _assert(leg_irq != 0 && leg_irq < IRQ_MAX);

    if (ioapic_available) {
        // Find out the route (TODO)
        uint8_t gsi = amd64_ioapic_leg_gsi(leg_irq);
        return irq_add_handler(gsi, handler);
    } else {
        irq_handler_t *list = &handlers[IRQ_MAX_HANDLERS * leg_irq];

        for (size_t i = 0; i < IRQ_MAX_HANDLERS; ++i) {
            if (!list[i]) {
                list[i] = handler;
                return 0;
            }
        }

        return -1;
    }
}

int irq_add_pci_handler(pci_addr_t addr, uint8_t pin, irq_handler_t handler) {
    _assert(pin < 4);
    uint32_t irq_route = amd64_pci_pin_irq_route(PCI_BUS(addr), PCI_DEV(addr), PCI_FUNC(addr), pin);
    _assert(irq_route != PCI_IRQ_INVALID);

    if (irq_route == PCI_IRQ_NO_ROUTE) {
        panic("TODO: allocate PCI IRQ routes\n");
    }

    kdebug("Assigning handler to " PCI_FMTADDR " INT%c# -> GSI%d", PCI_VAADDR(addr), pin + 'A', irq_route);

    return irq_add_handler(irq_route, handler);
}

void irq_enable_ioapic_mode(void) {
    ioapic_available = 1;
    // Copy current table
    irq_handler_t handlers_old[16 * IRQ_MAX_HANDLERS];

    memcpy(handlers_old, handlers, 16 * IRQ_MAX_HANDLERS * sizeof(irq_handler_t));
    memset(handlers, 0, 16 * IRQ_MAX_HANDLERS * sizeof(irq_handler_t));

    // All legacy IRQs
    for (size_t i = 0; i < 16; ++i) {
        irq_handler_t *handler_list = &handlers_old[i * IRQ_MAX_HANDLERS];

        for (size_t j = 0; j < IRQ_MAX_HANDLERS; ++j) {
            if (handler_list[j]) {
                irq_add_leg_handler(i, handlers_old[i * IRQ_MAX_HANDLERS + j]);
            }
        }
    }
}

void irq_init(void) {
    amd64_idt_set(32, (uintptr_t) amd64_irq0, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(33, (uintptr_t) amd64_irq1, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(34, (uintptr_t) amd64_irq2, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(35, (uintptr_t) amd64_irq3, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(36, (uintptr_t) amd64_irq4, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(37, (uintptr_t) amd64_irq5, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(38, (uintptr_t) amd64_irq6, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(39, (uintptr_t) amd64_irq7, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(40, (uintptr_t) amd64_irq8, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(41, (uintptr_t) amd64_irq9, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(42, (uintptr_t) amd64_irq10, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(43, (uintptr_t) amd64_irq11, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(44, (uintptr_t) amd64_irq12, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(45, (uintptr_t) amd64_irq13, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(46, (uintptr_t) amd64_irq14, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(47, (uintptr_t) amd64_irq15, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
}
