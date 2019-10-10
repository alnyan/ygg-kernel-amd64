#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/ioapic.h"
#include "sys/amd64/hw/timer.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/mm/mm.h"
#include "sys/string.h"
#include "sys/debug.h"

#define IA32_APIC_BASE_MSR          0x1B
#define IA32_APIC_BASE_MSR_BSP      0x100
#define IA32_APIC_BASE_MSR_ENABLE   0x800

#define IA32_LAPIC_REG_ID           0x20
#define IA32_LAPIC_REG_TPR          0x80
#define IA32_LAPIC_REG_EOI          0xB0
#define IA32_LAPIC_REG_SVR          0xF0

#define IA32_LAPIC_REG_CMD0         0x300
#define IA32_LAPIC_REG_CMD1         0x310

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
static uintptr_t lapic_base;

extern char kernel_stacks_top[];
// TODO: use mutual exclusion for this
static size_t started_up_aps = 0;

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
static void amd64_ap_code_entry(void) {
    // Can do this as core should've bootstrapped BEFORE BSP checks this value again
    // {{{ CRITICAL BLOCK
    ++started_up_aps;
    // }}}

    kdebug("AP %d startup\n", started_up_aps);

    // Enable LAPIC.SVR.SoftwareEnable bit
    // And set spurious interrupt mapping to 0xFF
    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_SVR) |= (1 << 8) | (0xFF);

    // Enable LAPIC timer
    amd64_timer_init();

    while (1) {
        asm ("sti; hlt");
    }
}

static void amd64_load_ap_code(void) {
    extern const char amd64_ap_code_start[];
    extern const char amd64_ap_code_end[];
    //extern void amd64_ap_code_entry(void);
    size_t ap_code_size = (uintptr_t) amd64_ap_code_end - (uintptr_t) amd64_ap_code_start;

    // Startup vector physical address, below 1M boundary
    uintptr_t physical_address = 0x7000;

    // Load code at 0x7000
    memcpy((void *) (0xFFFFFF0000000000 + 0x7000), amd64_ap_code_start, ap_code_size);

    // These parameters are shared and may be loaded only once
    // Write AP code startup parameters
    extern mm_space_t mm_kernel;
    uintptr_t mm_kernel_phys = (uintptr_t) mm_kernel - 0xFFFFFF0000000000;
    uintptr_t amd64_gdtr_phys = (uintptr_t) &amd64_gdtr - 0xFFFFFF0000000000;

    // 0x7FC0 - MM_PHYS(mm_kernel)
    *((uint64_t *) 0xFFFFFF0000007FC0) = mm_kernel_phys;
    // 0x7FC8 - MM_PHYS(amd64_gdtr)
    *((uint64_t *) 0xFFFFFF0000007FC8) = amd64_gdtr_phys;
    // 0x7FD0 - amd64_idtr
    *((uint64_t *) 0xFFFFFF0000007FD0) = (uintptr_t) &amd64_idtr;
    // 0x7FD8 - amd64_core_entry
    *((uint64_t *) 0xFFFFFF0000007FD8) = (uintptr_t) amd64_ap_code_entry;
}

static void amd64_set_ap_params(void) {
    // Allocate a new AP kernel stack
    uintptr_t stack_ptr = (uintptr_t) kernel_stacks_top - started_up_aps * 65536;
    // 0x7FE0 - stack_ptr
    *((uint64_t *) 0xFFFFFF0000007FE0) = stack_ptr;
}

static void amd64_core_wakeup(uint8_t core_id) {
    kdebug("Waking up LAPIC ID %d\n", core_id);

    amd64_set_ap_params();

    uint8_t entry_vector = 0x7000 >> 12;

    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_CMD1) = ((uint32_t) core_id) << 24;
    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_CMD0) = entry_vector | (5 << 8) | (1 << 14);

    for (uint64_t i = 0; i < 1000000; ++i);

    size_t old_ap_count = started_up_aps;

    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_CMD1) = ((uint32_t) core_id) << 24;
    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_CMD0) = entry_vector | (6 << 8) | (1 << 14);


    for (uint64_t i = 0; i < 10000000; ++i);

    if (started_up_aps == old_ap_count) {
        kdebug("AP failed to start: LAPIC ID %d\n", core_id);
        while (1) {
            asm ("cli; hlt");
        };
    }
}

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
                    amd64_core_wakeup(ent->apic_id);
                }
            }
        }

        offset += ent_hdr->length;
    }
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
    lapic_base = amd64_apic_base() + 0xFFFFFF0000000000;
    kdebug("APIC base is %p\n", lapic_base);
    kdebug("APIC is %s\n", (amd64_apic_status() ? "enabled" : "disabled"));

    kdebug("Disabling i8259 PIC\n");
    amd64_pic8259_disable();

    // Determine the BSP LAPIC ID
    bsp_lapic_id = (*(uint32_t *) (lapic_base + IA32_LAPIC_REG_ID) >> 24);
    kdebug("BSP Local APIC ID: %d\n", bsp_lapic_id);

    // Enable LAPIC.SVR.SoftwareEnable bit
    // And set spurious interrupt mapping to 0xFF
    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_SVR) |= (1 << 8) | (0xFF);

    amd64_acpi_ioapic(madt);
#if defined(AMD64_SMP)
    amd64_acpi_smp(madt);
#endif

    amd64_timer_init();
}
