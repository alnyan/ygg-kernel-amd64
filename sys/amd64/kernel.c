#include "sys/debug.h"
#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/asm/asm_irq.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/cpu.h"
#include "sys/amd64/hw/vesa.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/mm/mm.h"
#include "sys/amd64/smp/smp.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/pci/pci.h"
#include "sys/amd64/hw/rs232.h"
#include "sys/amd64/hw/ps2.h"
#include "sys/amd64/hw/rtc.h"
#include "sys/amd64/hw/con.h"
#include "sys/amd64/fpu.h"
#include "sys/fs/sysfs.h"
#include "sys/config.h"
#include "sys/string.h"
#include "sys/random.h"
#include "sys/time.h"
#include "sys/panic.h"
#include "sys/assert.h"
#include "sys/fs/vfs.h"
#include "sys/blk/ram.h"
#include "sys/fs/tar.h"
#include "sys/tty.h"

static multiboot_info_t *multiboot_info;

void kernel_main(struct amd64_loader_data *data) {
    data = (struct amd64_loader_data *) MM_VIRTUALIZE(data);
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(data->multiboot_info_ptr);

    // Parse kernel command line
    kernel_set_cmdline(data->cmdline);

    // Reinitialize RS232 properly
    rs232_init(RS232_COM1);

    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr),
                          multiboot_info->mmap_length);

    amd64_gdt_init();
    amd64_idt_init(0);
    amd64_mm_init(data);

    amd64_acpi_init();
    amd64_con_init();

    // Print kernel version now
    kinfo("yggdrasil " KERNEL_VERSION_STR "\n");

    amd64_apic_init();

    while (1) {
        asm volatile ("sti; hlt");
    }
}
