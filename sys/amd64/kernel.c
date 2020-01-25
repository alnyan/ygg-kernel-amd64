#include "sys/debug.h"
#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/hw/gdt.h"
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
#include "sys/sched.h"
#include "sys/tty.h"

static multiboot_info_t *multiboot_info;

static void amd64_make_random_seed(void) {
    random_init(15267 + system_time);
}

void kernel_main(struct amd64_loader_data *data) {
    data = (struct amd64_loader_data *) MM_VIRTUALIZE(data);
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(data->multiboot_info_ptr);

    // Parse kernel command line
    kernel_set_cmdline(data->cmdline);

    // Reinitialize RS232 properly
    ps2_init();
    rs232_init(RS232_COM1);

    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr),
                          multiboot_info->mmap_length);

    amd64_gdt_init();
    amd64_idt_init();
    amd64_mm_init(data);
    // XXX: HEAP IS ONLY AVAILABLE AT THIS POINT
    // Register devices after heap is ready
    extern void pseudo_init();
    pseudo_init();
    ps2_register_device();

    // Add kernel version to sysfs for testing
    sysfs_add_config_endpoint("kernel.version", sizeof(KERNEL_VERSION_STR), KERNEL_VERSION_STR, sysfs_config_getter, NULL);

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

    pci_init();

    vfs_init();
    tty_init();
    if (data->initrd_ptr) {
        // Create ram0 block device
        ramblk_init(MM_VIRTUALIZE(data->initrd_ptr), data->initrd_len);
        tarfs_init();
    }

    // Initial random seed
    amd64_make_random_seed();

    amd64_fpu_init();

#if defined(AMD64_SMP)
    amd64_smp_init();
#endif
    sched_init();

    amd64_syscall_init();

    while (1) {
        asm ("sti; hlt");
    }
}
