#include "sys/amd64/hw/apic.h"
#include "sys/amd64/smp/smp.h"
#include "sys/amd64/hw/ioapic.h"
#include "sys/amd64/hw/timer.h"
#include "sys/string.h"
#include "sys/debug.h"

/////

static uint64_t rdmsr(uint32_t addr) {
    uint64_t v;
    asm volatile ("rdmsr":"=A"(v):"c"(addr));
    return v;
}

/////

struct acpi_apic_field_type {
    uint8_t type;
    uint8_t length;
} __attribute__((packed));

struct acpi_lapic_entry {
    struct acpi_apic_field_type hdr;

    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed));

struct acpi_ioapic_entry {
    struct acpi_apic_field_type hdr;

    uint8_t ioapic_id;
    uint8_t res;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} __attribute__((packed));

struct acpi_int_src_override_entry {
    struct acpi_apic_field_type hdr;

    uint8_t bus_src;
    uint8_t irq_src;
    uint32_t gsi;
    uint16_t flags;
} __attribute__((packed));

/////

// LAPIC ID of the BSP CPU
static uint32_t bsp_lapic_id;
// Local APIC base for every processor
// (mapped to per-CPU LAPICs, while the address is the same)
uintptr_t local_apic;

/////

static uintptr_t amd64_apic_base(void) {
    uint64_t addr = rdmsr(IA32_APIC_BASE_MSR);
    return addr & 0xFFFFFF000;
}

static int amd64_apic_status(void) {
    uintptr_t v = rdmsr(IA32_APIC_BASE_MSR);
    return v & IA32_APIC_BASE_MSR_ENABLE;
}

static void amd64_pic8259_disable(void) {
    // Mask everything
    asm volatile (
        "mov $(0x01 | 0x10), %al \n"
        "outb %al, $0x20 \n"
        "outb %al, $0xA0 \n"
        "mov $32, %al \n"
        "outb %al, $0x21 \n"
        "mov $(32 + 8), %al \n"
        "outb %al, $0xA1 \n"
        "mov $0xFF, %al \n"
        "outb %al, $0xA1 \n"
        "outb %al, $0x21"
    );
}

#if defined(AMD64_SMP)
void amd64_acpi_smp(struct acpi_madt *madt) {
    // Load the code APs are expected to run
    amd64_load_ap_code();
    // Get other LAPICs from MADT
    size_t offset = 0;
    size_t ncpu = 1;
    size_t ncpu_real = 1;

    while (offset < madt->hdr.length - sizeof(struct acpi_madt)) {
        struct acpi_apic_field_type *ent_hdr = (struct acpi_apic_field_type *) &madt->entry[offset];

        if (ent_hdr->type == 0) {
            // LAPIC entry
            struct acpi_lapic_entry *ent = (struct acpi_lapic_entry *) ent_hdr;

            // It's not us
            if (ent->apic_id != bsp_lapic_id) {
                ++ncpu_real;

                if (ncpu == AMD64_MAX_SMP) {
                    kwarn("Kernel does not support more than %d CPUs (Skipping %d)\n", ncpu, ncpu_real);
                } else {
                    // Initiate wakeup sequence
                    ++ncpu;
                    amd64_smp_add(ent->apic_id);
                }
            }
        }

        offset += ent_hdr->length;
    }

    amd64_smp_init();
}
#endif

void amd64_acpi_ioapic(struct acpi_madt *madt) {
    size_t offset = 0;

    while (offset < madt->hdr.length - sizeof(struct acpi_madt)) {
        struct acpi_apic_field_type *ent_hdr = (struct acpi_apic_field_type *) &madt->entry[offset];
        if (ent_hdr->type == 1) {
            // Found I/O APIC
            struct acpi_ioapic_entry *ent = (struct acpi_ioapic_entry *) ent_hdr;
            amd64_ioapic_set(ent->ioapic_addr + 0xFFFFFF0000000000);
        }

        offset += ent_hdr->length;
    }

    offset = 0;
    while (offset < madt->hdr.length - sizeof(struct acpi_madt)) {
        struct acpi_apic_field_type *ent_hdr = (struct acpi_apic_field_type *) &madt->entry[offset];
        if (ent_hdr->type == 2) {
            struct acpi_int_src_override_entry *ent = (struct acpi_int_src_override_entry *) ent_hdr;
            amd64_ioapic_int_src_override(ent->bus_src, ent->irq_src, ent->gsi, ent->flags);
        }

        offset += ent_hdr->length;
    }
}

void amd64_apic_init(struct acpi_madt *madt) {
    // Get LAPIC base
    local_apic = amd64_apic_base() + 0xFFFFFF0000000000;
    kdebug("APIC base is %p\n", local_apic);
    kdebug("APIC is %s\n", (amd64_apic_status() ? "enabled" : "disabled"));

    kdebug("Disabling i8259 PIC\n");
    amd64_pic8259_disable();

    // Determine the BSP LAPIC ID
    bsp_lapic_id = LAPIC(LAPIC_REG_ID) >> 24;
    kdebug("BSP Local APIC ID: %d\n", bsp_lapic_id);

    // Enable LAPIC.SVR.SoftwareEnable bit
    // And set spurious interrupt mapping to 0xFF
    LAPIC(LAPIC_REG_SVR) |= (1 << 8) | (0xFF);

    amd64_acpi_ioapic(madt);
#if defined(AMD64_SMP)
    amd64_acpi_smp(madt);
#endif

    amd64_timer_init();
}
