#pragma once
#include "sys/amd64/hw/pci/pci.h"
#include "sys/types.h"

#define IRQ_LEG_KEYBOARD        1
#define IRQ_LEG_COM1_3          4

#define IRQ_HANDLED             ((uint32_t) 0)
#define IRQ_UNHANDLED           ((uint32_t) -1)

typedef uint32_t (*irq_handler_func_t) (void *);

struct irq_handler {
    irq_handler_func_t func;
    void *ctx;
};

int irq_add_handler(uint8_t gsi, irq_handler_func_t handler, void *ctx);
int irq_add_leg_handler(uint8_t leg_irq, irq_handler_func_t handler, void *ctx);
int irq_add_pci_handler(pci_addr_t addr, uint8_t pin, irq_handler_func_t handler, void *ctx);

int irq_has_handler(uint8_t gsi);

void irq_enable_ioapic_mode(void);
void irq_init(void);
