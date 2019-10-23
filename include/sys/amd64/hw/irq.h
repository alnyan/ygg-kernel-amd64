#pragma once
#include "sys/amd64/hw/pci/pci.h"
#include "sys/types.h"

#define IRQ_LEG_KEYBOARD        1
#define IRQ_LEG_COM1_3          4

typedef int (*irq_handler_t) (void);

int irq_add_handler(uint8_t gsi, irq_handler_t handler);
int irq_add_leg_handler(uint8_t leg_irq, irq_handler_t handler);
int irq_add_pci_handler(pci_addr_t addr, uint8_t pin, irq_handler_t handler);

void irq_enable_ioapic_mode(void);
void irq_init(void);
