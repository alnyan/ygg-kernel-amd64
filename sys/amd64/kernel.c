#include "sys/amd64/hw/rs232.h"
#include "sys/driver/pci/pci.h"
#include "sys/driver/usb/usb.h"
#include "sys/amd64/hw/apic.h"
#include "sys/amd64/hw/acpi.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/syscall.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/hw/con.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/hw/rtc.h"
#include "sys/amd64/hw/ps2.h"
#include "sys/amd64/cpuid.h"
#include "sys/amd64/mm/mm.h"
#include "sys/block/ram.h"
#include "sys/fs/sysfs.h"
#include "sys/char/tty.h"
#include "sys/config.h"
#include "sys/fs/tar.h"
#include "sys/fs/vfs.h"
#include "sys/random.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/init.h"
#include "sys/mm.h"

static multiboot_info_t *multiboot_info;

static void amd64_make_random_seed(void) {
    random_init(15267 + system_time);
}

void kernel_main(struct amd64_loader_data *data) {
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
    amd64_con_init();
    rs232_init(RS232_COM1);
    ps2_init();

    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr),
                          multiboot_info->mmap_length);

    amd64_gdt_init();
    amd64_idt_init(0);
    amd64_mm_init(data);

    ps2_register_device();

    amd64_acpi_init();

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

    amd64_make_random_seed();

    syscall_init();

    sched_init();
    sysfs_populate();
    usb_daemon_start();
    user_init_start();
    sched_enter();

    panic("This code should not run\n");
}
