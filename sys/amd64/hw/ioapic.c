#include "sys/amd64/hw/ioapic.h"
#include "sys/string.h"
#include "sys/mm.h"
#include "sys/debug.h"

struct pir_slot_entry {
    uint8_t pci_bus;
    uint8_t pci_dev;
    uint8_t inta_link;
    uint16_t inta_bitmap;
    uint8_t intb_link;
    uint16_t intb_bitmap;
    uint8_t intc_link;
    uint16_t intc_bitmap;
    uint8_t intd_link;
    uint16_t intd_bitmap;
    uint8_t slot_number;
    char __res0;
} __attribute__((packed));

struct pir_table {
    char signature[4];
    uint16_t version;
    uint16_t table_size;
    uint8_t pci_router_bus;
    uint8_t pci_router_dev_func;
    uint16_t pci_exc_irqs;
    uint32_t compat_pci_router;
    uint32_t miniport_data;
    char __res0[11];
    uint8_t checksum;
    struct pir_slot_entry entries[];
} __attribute__((packed));

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

static void amd64_pir_dump(struct pir_table *table) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < table->table_size; ++i) {
        checksum += ((uint8_t *) table)[i];
    }
    if (checksum != 0) {
        kerror("$PIR checksum is invalid\n");
        return;
    }
    size_t nentr = (table->table_size - sizeof(struct pir_table)) / sizeof(struct pir_slot_entry);

    kdebug("$PIR: %lu entries:\n", nentr);
    kdebug(" #   BUS DEV PIN LINK IRQs\n");
    for (size_t i = 0; i < nentr; ++i) {
        struct pir_slot_entry *ent = &table->entries[i];
        uint8_t pin = 0xFF;
        uint8_t link = 0x00;
        uint16_t bitmap = 0x0000;

        if (ent->inta_link != 0) {
            pin = 0;
            link = ent->inta_link;
            bitmap = ent->inta_bitmap;
        } else if (ent->intb_link != 0) {
            pin = 1;
            link = ent->intb_link;
            bitmap = ent->intb_bitmap;
        } else if (ent->intc_link != 0) {
            pin = 2;
            link = ent->intc_link;
            bitmap = ent->intc_bitmap;
        } else if (ent->intd_link != 0) {
            pin = 3;
            link = ent->intd_link;
            bitmap = ent->intd_bitmap;
        }
        kdebug("[%02d] %02d  %02d  %2c   %02x   ",
                i,
                ent->pci_bus,
                ent->pci_dev >> 3,
                (pin == 0xFF ? '?' : 'A' + pin),
                link);
        for (uint8_t i = 0; i < 16; ++i) {
            if (bitmap & (1 << i)) {
                // Weird ways
                char n[3] = {0};
                if (i > 10) {
                    n[1] = i % 10 + '0';
                    n[0] = i / 10 + '0';
                } else {
                    n[0] = i % 10 + '0';
                }
                debugs(DEBUG_DEFAULT, n);
                debugc(DEBUG_DEFAULT, ' ');
            }
        }
        debugc(DEBUG_DEFAULT, '\n');
    }
}

static int amd64_find_pir_table(void) {
    for (uintptr_t addr = MM_VIRTUALIZE(0xF0000); addr < MM_VIRTUALIZE(0x100000); ++addr) {
        if (!strncmp((const char *) addr, "$PIR", 4)) {
            kdebug("Found $PIR @ %p\n", addr);

            amd64_pir_dump((struct pir_table *) addr);
            break;
        }
    }
    return 0;
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

    amd64_find_pir_table();
}
