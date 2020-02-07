#include "arch/amd64/hw/rs232.h"
#include "arch/amd64/hw/vesa.h"
#include "arch/amd64/hw/apic.h"
#include "arch/amd64/smp/smp.h"
#include "arch/amd64/hw/acpi.h"
#include "arch/amd64/mm/phys.h"
#include "arch/amd64/hw/gdt.h"
#include "arch/amd64/hw/con.h"
#include "arch/amd64/hw/idt.h"
#include "arch/amd64/hw/rtc.h"
#include "arch/amd64/hw/ps2.h"
#include "arch/amd64/cpuid.h"
#include "arch/amd64/mm/mm.h"
#include "arch/amd64/fpu.h"
#include "sys/block/ram.h"
#include "sys/config.h"
#include "sys/kernel.h"
#include "sys/random.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/mm.h"

static multiboot_info_t *multiboot_info;

static void amd64_make_random_seed(void) {
    random_init(15267 + system_time);
}

void kernel_early_init(struct amd64_loader_data *data) {
    cpuid_init();
    data = (struct amd64_loader_data *) MM_VIRTUALIZE(data);
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(data->multiboot_info_ptr);

    if (data->symtab_ptr) {
        kinfo("Kernel symbol table at %p\n", MM_VIRTUALIZE(data->symtab_ptr));
        debug_symbol_table_set(MM_VIRTUALIZE(data->symtab_ptr), MM_VIRTUALIZE(data->strtab_ptr), data->symtab_size, data->strtab_size);
    }

    // Parse kernel command line
    kernel_set_cmdline(data->cmdline);

    // Reinitialize RS232 properly
    rs232_init(RS232_COM1);
    ps2_init();

    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr),
                          multiboot_info->mmap_length);

    amd64_gdt_init();
    amd64_idt_init(0);
    amd64_mm_init(data);

    ps2_register_device();

    amd64_acpi_init();
#if defined(VESA_ENABLE)
    amd64_vesa_init(multiboot_info);
#endif
    amd64_con_init();

    // Print kernel version now
    kinfo("yggdrasil " KERNEL_VERSION_STR "\n");

    amd64_apic_init();
    rtc_init();
    // Setup system time
    struct tm t;
    rtc_read(&t);
    system_boot_time = mktime(&t);
    kinfo("Boot time: %04u-%02u-%02u %02u:%02u:%02u\n",
        t.tm_year, t.tm_mon, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec);

    if (data->initrd_ptr) {
        // Create ram0 block device
        ramblk_init(MM_VIRTUALIZE(data->initrd_ptr), data->initrd_len);
    }

    amd64_make_random_seed();

    amd64_fpu_init();

#if defined(AMD64_SMP)
    amd64_smp_init();
#endif
}

void kernel_main(struct amd64_loader_data *data) {
    kernel_early_init(data);
    main();
}
