#include "arch/amd64/multiboot2.h"
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

static uintptr_t multiboot_info_addr;
static struct multiboot_tag_mmap    *multiboot_tag_mmap;
static struct multiboot_tag_module  *multiboot_tag_initrd_module;
static struct multiboot_tag_string  *multiboot_tag_cmdline;

extern struct {
    uint32_t eax, ebx;
} __attribute__((packed)) multiboot_registers;

static void amd64_make_random_seed(void) {
    random_init(15267 + system_time);
}

void kernel_early_init(void) {
    // Check Multiboot2 signature
    if (multiboot_registers.eax != MULTIBOOT2_BOOTLOADER_MAGIC) {
        panic("Invalid bootloader magic\n");
    }
    multiboot_info_addr = MM_VIRTUALIZE(multiboot_registers.ebx);

    // Find all requested tags
    uint32_t multiboot_len = *(uint32_t *) multiboot_info_addr;
    size_t offset = 8; // Skip 2 fields
    while (offset < multiboot_len) {
        struct multiboot_tag *tag = (struct multiboot_tag *) (multiboot_info_addr + offset);

        if (tag->type == 0) {
            break;
        }

        switch (tag->type) {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
            multiboot_tag_cmdline = (struct multiboot_tag_string *) tag;
            break;
        case MULTIBOOT_TAG_TYPE_MMAP:
            multiboot_tag_mmap = (struct multiboot_tag_mmap *) tag;
            break;
        case MULTIBOOT_TAG_TYPE_MODULE:
            multiboot_tag_initrd_module = (struct multiboot_tag_module *) tag;
            break;
        default:
            kdebug("tag.type = %u, tag.size = %u\n", tag->type, tag->size);
            break;
        }

        offset += (tag->size + 7) & ~7;
    }

    if (!multiboot_tag_mmap) {
        panic("Multiboot2 loader provided no memory map\n");
    }

    cpuid_init();

    // TODO: use "ELF symbols" tag to get this info
    //if (data->symtab_ptr) {
    //    kinfo("Kernel symbol table at %p\n", MM_VIRTUALIZE(data->symtab_ptr));
    //    debug_symbol_table_set(MM_VIRTUALIZE(data->symtab_ptr), MM_VIRTUALIZE(data->strtab_ptr), data->symtab_size, data->strtab_size);
    //}

    if (multiboot_tag_cmdline) {
        // Set kernel command line
        kinfo("Provided command line: \"%s\"\n", multiboot_tag_cmdline->string);
        kernel_set_cmdline(multiboot_tag_cmdline->string);
    }

    // Reinitialize RS232 properly
    rs232_init(RS232_COM1);
    ps2_init();

    amd64_phys_memory_map(multiboot_tag_mmap);

    amd64_gdt_init();
    amd64_idt_init(0);
    amd64_mm_init();

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

    if (multiboot_tag_initrd_module) {
        // Create ram0 block device
        ramblk_init(MM_VIRTUALIZE(multiboot_tag_initrd_module->mod_start),
                    multiboot_tag_initrd_module->mod_end - multiboot_tag_initrd_module->mod_start);
    }

    amd64_make_random_seed();

    amd64_fpu_init();

#if defined(AMD64_SMP)
    amd64_smp_init();
#endif
}

void kernel_main(void) {
    kernel_early_init();
    main();
}
