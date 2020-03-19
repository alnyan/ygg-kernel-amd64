#pragma once
// TODO: lots of stuff can be moved to some kind of generic
//       irq.h header to provide controller-agnostic interface
#include "sys/types.h"

struct pci_device;

#define IOAPIC_REG_ID               0x00
#define IOAPIC_REG_VER              0x01
#define IOAPIC_REG_REDIR            0x10

#define IOAPIC_REDIR_DEL_NOPRI      (0x0 << 8)
#define IOAPIC_REDIR_DEL_LOPRI      (0x1 << 8)
#define IOAPIC_REDIR_DEL_SMI        (0x2 << 8)
#define IOAPIC_REDIR_DEL_NMI        (0x4 << 8)
#define IOAPIC_REDIR_DEL_INIT       (0x5 << 8)
#define IOAPIC_REDIR_DEL_EXTI       (0x7 << 8)

#define IOAPIC_REDIR_DST_LOG        (1 << 11)

#define IOAPIC_REDIR_POL_LOW        (1 << 13)

#define IOAPIC_REDIR_TRG_LEVEL      (1 << 15)

#define IOAPIC_REDIR_MSK            (1 << 16)

#define IOAPIC_REDIR_DST_SET(x)     (((x) & 0xFF) << 24)
#define IOAPIC_REDIR_DST_GET(x)     (((uint32_t) x) >> 24)

// Base address of the two-register structure
// 0 means "Not available"
// [Virtual]
extern uintptr_t amd64_ioapic_base;

uint32_t amd64_ioapic_read(uint8_t reg);
void amd64_ioapic_write(uint8_t reg, uint32_t v);

// Set from reading the MADT
void amd64_ioapic_set(uintptr_t addr);
void amd64_ioapic_int_src_override(uint8_t bus, uint8_t src, uint32_t no, uint16_t flags);

void amd64_ioapic_map_gsi(uint8_t gsi, uint8_t lapic, uint8_t vector);
void amd64_ioapic_unmask(uint8_t gsi);
void amd64_ioapic_mask(uint8_t gsi);

uint8_t amd64_ioapic_leg_gsi(uint8_t leg_irq);

//// Generic IRQ stuff
// A route exists (LNKx) but is not mapped to any IRQ at the moment
#define PCI_IRQ_NO_ROUTE            0xFFFFFFFF
// No route even exists
#define PCI_IRQ_INVALID             0xFFFFFFFE

int amd64_pci_init_irqs(void);
uint32_t amd64_pci_pin_irq_route(struct pci_device *dev, uint8_t pin);
