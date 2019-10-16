#include "sys/debug.h"
#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/mm/mm.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/pci/pci.h"
#include "sys/panic.h"

#include "acpi.h"

static multiboot_info_t *multiboot_info;

static ACPI_STATUS acpi_init_wrapper(void) {
    ACPI_STATUS ret;

    if (ACPI_FAILURE(ret = AcpiInitializeSubsystem())) {
        kerror("AcpiInitializeSubsystem failed\n");
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInitializeTables(NULL, 0, FALSE))) {
        kerror("AcpiInitializeTables failed\n");
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION))) {
        kerror("AcpiEnableSubsystem failed\n");
        return ret;
    }

    if (ACPI_FAILURE(ret = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION))) {
        kerror("AcpiInitializeObjects failed\n");
        return ret;
    }

    return AE_OK;
}

void kernel_main(struct amd64_loader_data *data) {
    data = (struct amd64_loader_data *) MM_VIRTUALIZE(data);
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(data->multiboot_info_ptr);

    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr),
                          multiboot_info->mmap_length);

    amd64_gdt_init();
    amd64_idt_init();
    amd64_mm_init(data);
    pci_enumerate();

    if (ACPI_FAILURE(acpi_init_wrapper())) {
        panic("ACPI initialization failed\n");
    }

    extern void sched_init(void);
    sched_init();
    amd64_acpi_init();

    amd64_syscall_init();

    while (1) {
        asm ("sti; hlt");
    }
}
