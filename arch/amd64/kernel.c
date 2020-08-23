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
#include "sys/mem/phys.h"
#include "sys/console.h"
#include "sys/config.h"
#include "sys/kernel.h"
#include "sys/random.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/syms.h"
#include "sys/attr.h"
#include "sys/elf.h"
#include "sys/mm.h"

extern char _kernel_start, _kernel_end;

static uintptr_t multiboot_info_addr;
static struct multiboot_tag_mmap            *multiboot_tag_mmap;
static struct multiboot_tag_module          *multiboot_tag_initrd_module;
static struct multiboot_tag_elf_sections    *multiboot_tag_sections;
static struct multiboot_tag_string          *multiboot_tag_cmdline;
static struct multiboot_tag_framebuffer     *multiboot_tag_framebuffer;

// Descriptors for reserved physical memory regions
static struct mm_phys_reserved              phys_reserve_initrd;
static struct mm_phys_reserved              phys_reserve_kernel = {
    // TODO: use _kernel_start instead of this
    // I was kinda lazy to add an additional reserved region for
    // multiboot stuff, so I simplified things a bit:
    // multiboot is known (don't know if it's a standard) to place
    // its structures below the kernel, so if I reserve pages below the
    // kernel, nothing should be overwritten
    .begin = 0,
    .end = MM_PHYS(&_kernel_end)
};

extern struct {
    uint32_t eax, ebx;
} __attribute__((packed)) multiboot_registers;

static void amd64_make_random_seed(void) {
    random_init(15267 + system_time);
}

void kernel_early_init(void) {
    // Allows early output
    amd64_console_init();

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
        case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
            multiboot_tag_sections = (struct multiboot_tag_elf_sections *) tag;
            break;
        case MULTIBOOT_TAG_TYPE_MMAP:
            multiboot_tag_mmap = (struct multiboot_tag_mmap *) tag;
            break;
        case MULTIBOOT_TAG_TYPE_MODULE:
            multiboot_tag_initrd_module = (struct multiboot_tag_module *) tag;
            break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
            multiboot_tag_framebuffer = (struct multiboot_tag_framebuffer *) tag;
            break;
        default:
            kdebug("tag.type = %u, tag.size = %u\n", tag->type, tag->size);
            break;
        }

        offset += (tag->size + 7) & ~7;
    }

    // Console can only be initialized after memory buffers can be allocated
#if defined(VESA_ENABLE)
    if (multiboot_tag_framebuffer) {
        // Early display init
        vesa_init(multiboot_tag_framebuffer);
    }
#endif

    if (!multiboot_tag_mmap) {
        panic("Multiboot2 loader provided no memory map\n");
    }

    cpuid_init();

    if (multiboot_tag_cmdline) {
        // Set kernel command line
        kinfo("Provided command line: \"%s\"\n", multiboot_tag_cmdline->string);
        kernel_set_cmdline(multiboot_tag_cmdline->string);
    }

    // Reinitialize RS232 properly
    rs232_init(RS232_COM1);
    ps2_init();

    // Before anything is allocated, reserve:
    // 1. initrd pages
    // 2. multiboot tag pages
    mm_phys_reserve(&phys_reserve_kernel);

    if (multiboot_tag_initrd_module) {
        phys_reserve_initrd.begin = multiboot_tag_initrd_module->mod_start & ~0xFFF;
        phys_reserve_initrd.end = (multiboot_tag_initrd_module->mod_end + 0xFFF) & ~0xFFF;
        mm_phys_reserve(&phys_reserve_initrd);
    }

    amd64_phys_memory_map(multiboot_tag_mmap);

    amd64_gdt_init();
    amd64_idt_init(0);

    amd64_mm_init();

    if (multiboot_tag_sections) {
        ksym_set_multiboot2(multiboot_tag_sections);
    }

    if (rs232_avail & (1 << 0)) {
        rs232_add_tty(0);
    }

    console_init_default();

    ps2_register_device();

    amd64_acpi_init();

    // Print kernel version now
    kinfo("yggdrasil " KERNEL_VERSION_STR "\n");

    if (!multiboot_tag_sections) {
        kwarn("No ELF sections provided, module loading is unavailable\n");
    }

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
