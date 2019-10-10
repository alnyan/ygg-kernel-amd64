#include "sys/amd64/hw/ioapic.h"
#include "sys/debug.h"

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

    kdebug("I/O APIC @ %p\n", addr);

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

        kdebug("[%d] %s, %s, DST 0x%02x:0x%02x\n",
               i,
               (low & IOAPIC_REDIR_MSK) ? "Masked" : "",
               del_type[(low >> 8) & 0x7],
               IOAPIC_REDIR_DST_GET(high),
               low & 0xF);
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
}
