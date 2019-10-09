#include "sys/amd64/hw/apic.h"
#include "sys/debug.h"

#define IA32_APIC_BASE_MSR          0x1B
#define IA32_APIC_BASE_MSR_BSP      0x100
#define IA32_APIC_BASE_MSR_ENABLE   0x800

#define IA32_LAPIC_REG_ID           0x20
#define IA32_LAPIC_REG_TPR          0x80
#define IA32_LAPIC_REG_EOI          0xB0
#define IA32_LAPIC_REG_SVR          0xF0

#define IA32_LAPIC_REG_LVTT         0x320

#define IA32_LAPIC_REG_TMRINITCNT   0x380
#define IA32_LAPIC_REG_TMRCURRCNT   0x390
#define IA32_LAPIC_REG_TMRDIV       0x3E0

#define IA32_LAPIC_REG_CMD0         0x300
#define IA32_LAPIC_REG_CMD1         0x310

/////

static uint64_t rdmsr(uint32_t addr) {
    uint64_t v;
    asm volatile ("rdmsr":"=A"(v):"c"(addr));
    return v;
}

// static void wrmsr(uint32_t addr, uint64_t v) {
//     uint32_t low = v & 0xFFFFFFFF;
//     uint32_t high = v >> 32;
//     asm volatile ("wrmsr"::"c"(addr),"d"(high),"a"(low));
// }

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

/////

// LAPIC ID of the BSP CPU
static uint32_t bsp_lapic_id;
// Local APIC base for every processor
// (mapped to per-CPU LAPICs, while the address is the same)
static uintptr_t lapic_base;

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
        "mov $0xFF, %al \n"
        "outb %al, $0xA1 \n"
        "outb %al, $0x21"
    );
}

void amd64_apic_init(struct acpi_madt *madt) {
    // Get LAPIC base
    lapic_base = amd64_apic_base() + 0xFFFFFF0000000000;
    kdebug("APIC base is %p\n", lapic_base);
    kdebug("APIC is %s\n", (amd64_apic_status() ? "enabled" : "disabled"));

    kdebug("Disabling i8259 PIC\n");
    amd64_pic8259_disable();

    // Determine the BSP LAPIC ID
    bsp_lapic_id = *(uint32_t *) (lapic_base + IA32_LAPIC_REG_ID);
    kdebug("BSP Local APIC ID: %d\n", bsp_lapic_id);

    // Enable LAPIC.SVR.SoftwareEnable bit
    // And set spurious interrupt mapping to 0xFF
    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_SVR) |= (1 << 8) | (0xFF);

    // Enable LAPIC timer
    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_LVTT) = 32 | 0x20000;
    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_TMRDIV) = 0x3;
    *(uint32_t *) (lapic_base + IA32_LAPIC_REG_TMRINITCNT) = 100000;
}
