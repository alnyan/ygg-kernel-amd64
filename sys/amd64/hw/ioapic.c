#include "sys/amd64/hw/ioapic.h"
#include "sys/amd64/hw/irq.h"
#include "sys/string.h"
#include "sys/mm.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/assert.h"
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
    memset(ioapic_leg_rt, 0xFF, sizeof(ioapic_leg_rt));
}

void amd64_ioapic_map_gsi(uint8_t gsi, uint8_t lapic, uint8_t vector) {
    uint32_t low = amd64_ioapic_read((gsi * 2) + 0x10);
    uint32_t high = amd64_ioapic_read((gsi * 2) + 0x11);

    low &= (~0xFF) & (IOAPIC_REDIR_POL_LOW | IOAPIC_REDIR_TRG_LEVEL);
    low |= vector;

    high = IOAPIC_REDIR_DST_SET(lapic);

    amd64_ioapic_write((gsi * 2) + 0x10, low);
    amd64_ioapic_write((gsi * 2) + 0x11, high);
}

void amd64_ioapic_mask(uint8_t gsi) {
    uint32_t low = amd64_ioapic_read((gsi * 2) + 0x10);

    low |= IOAPIC_REDIR_MSK;

    amd64_ioapic_write((gsi * 2) + 0x10, low);
}

void amd64_ioapic_unmask(uint8_t gsi) {
    uint32_t low = amd64_ioapic_read((gsi * 2) + 0x10);

    low &= ~IOAPIC_REDIR_MSK;

    amd64_ioapic_write((gsi * 2) + 0x10, low);
}

uint8_t amd64_ioapic_leg_gsi(uint8_t leg_irq) {
    _assert(leg_irq && leg_irq < 16);
    if (ioapic_leg_rt[leg_irq] != 0xFFFFFFFF) {
        return ioapic_leg_rt[leg_irq];
    }
    return leg_irq;
}

