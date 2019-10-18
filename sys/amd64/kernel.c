#include "sys/debug.h"
#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/mm/mm.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/pci/pci.h"
#include "sys/panic.h"
#include "sys/assert.h"

#include "acpi.h"

static multiboot_info_t *multiboot_info;

void kernel_main(struct amd64_loader_data *data) {
    data = (struct amd64_loader_data *) MM_VIRTUALIZE(data);
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(data->multiboot_info_ptr);

    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr),
                          multiboot_info->mmap_length);

    amd64_gdt_init();
    amd64_idt_init();
    amd64_mm_init(data);
    amd64_acpi_init();

    pci_init();

    extern void sched_init(void);
    sched_init();
    amd64_apic_init();

    amd64_syscall_init();

    while (1) {
        asm ("sti; hlt");
    }
}
