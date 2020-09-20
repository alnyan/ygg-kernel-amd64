#include "arch/amd64/multiboot2.h"
#include "arch/amd64/boot/yboot.h"
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
#include "sys/assert.h"
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

static struct elf_sections elf_sections;
static uintptr_t initrd_phys_start;
static size_t initrd_size;

static struct boot_video_info boot_video_info = {0};

// Descriptors for reserved physical memory regions
static struct mm_phys_memory_map            phys_memory_map;
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

static void amd64_make_random_seed(void) {
    random_init(15267 + system_time);
}

static void entry_multiboot(void) {
    extern struct {
        uint32_t eax, ebx;
    } __attribute__((packed)) multiboot_registers;

    uintptr_t multiboot_info_addr;
    struct multiboot_tag_framebuffer *multiboot_tag_framebuffer = NULL;
    struct multiboot_tag_module *multiboot_tag_initrd_module = NULL;
    struct multiboot_tag_string *multiboot_tag_cmdline = NULL;
    struct multiboot_tag_mmap *multiboot_tag_mmap = NULL;

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
            elf_sections.kind = KSYM_TABLE_MULTIBOOT2;
            elf_sections.tables.multiboot2 = (struct multiboot_tag_elf_sections *) tag;
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

    if (multiboot_tag_cmdline) {
        // Set kernel command line
        kinfo("Provided command line: \"%s\"\n", multiboot_tag_cmdline->string);
        kernel_set_cmdline(multiboot_tag_cmdline->string);
    }

    if (!multiboot_tag_mmap) {
        panic("Multiboot2 provided no memory map\n");
    }

    if (multiboot_tag_initrd_module) {
        initrd_phys_start = multiboot_tag_initrd_module->mod_start;
        initrd_size = multiboot_tag_initrd_module->mod_end -
                      multiboot_tag_initrd_module->mod_start;
    }

    if (multiboot_tag_framebuffer) {
        struct multiboot_tag_framebuffer_common *info = &multiboot_tag_framebuffer->common;

        if (info->framebuffer_type == 1) {
            boot_video_info.width = info->framebuffer_width;
            boot_video_info.height = info->framebuffer_height;
            boot_video_info.bpp = info->framebuffer_bpp;
            boot_video_info.pitch = info->framebuffer_pitch;
            boot_video_info.framebuffer_phys = info->framebuffer_addr;
        }
    }

    phys_memory_map.format = MM_PHYS_MMAP_FMT_MULTIBOOT2;
    phys_memory_map.address = multiboot_tag_mmap->entries;
    phys_memory_map.entry_size = multiboot_tag_mmap->entry_size;
    // TODO: I'm not really sure here
    phys_memory_map.entry_count = (multiboot_tag_mmap->size -
        offsetof(struct multiboot_tag_mmap, entries)) / multiboot_tag_mmap->entry_size;
}

static void entry_yboot(void) {
    if (yboot_data.rsdp == 0) {
        kwarn("Booted from UEFI and no RSDP was provided, will likely result in error\n");
    } else {
        amd64_acpi_set_rsdp(MM_VIRTUALIZE(yboot_data.rsdp));
    }

    kinfo("Provided command line: \"%s\"\n", yboot_data.cmdline);
    kernel_set_cmdline(yboot_data.cmdline);

    if (yboot_data.video_framebuffer) {
        boot_video_info.width = yboot_data.video_width;
        boot_video_info.height = yboot_data.video_height;
        boot_video_info.bpp = 32;
        boot_video_info.pitch = yboot_data.video_pitch;
        boot_video_info.framebuffer_phys = yboot_data.video_framebuffer;
    }

    if (yboot_data.initrd_base) {
        kdebug("INITRD BASE: %p, INITRD SIZE: %S\n", yboot_data.initrd_base, yboot_data.initrd_size);
        initrd_phys_start = yboot_data.initrd_base;
        initrd_size = yboot_data.initrd_size;
    }

    phys_memory_map.format = MM_PHYS_MMAP_FMT_YBOOT;
    phys_memory_map.address = (void *) MM_VIRTUALIZE(yboot_data.memory_map_data);
    phys_memory_map.entry_size = yboot_data.memory_map_entsize;
    phys_memory_map.entry_count = yboot_data.memory_map_size / yboot_data.memory_map_entsize;
}

void kernel_early_init(uint64_t entry_method) {
    // Allows early output
    amd64_console_init();

    switch (entry_method) {
    case 0:
        entry_multiboot();
        break;
    case 1:
        entry_yboot();
        break;
    default:
        panic("Unknown boot method: something's broken\n");
        break;
    }
    cpuid_init();

    vesa_early_init(&boot_video_info);
    struct display *disp = vesa_get_display();
    if (disp) {
        kdebug("Initialize early console!\n");
        console_init_early(disp);
    }

    // Reinitialize RS232 properly
    rs232_init(RS232_COM1);
    ps2_init();

    // Before anything is allocated, reserve:
    // 1. initrd pages
    // 2. multiboot tag pages
    mm_phys_reserve("Kernel", &phys_reserve_kernel);

    if (initrd_phys_start) {
        phys_reserve_initrd.begin = initrd_phys_start & ~0xFFF;
        phys_reserve_initrd.end = (initrd_phys_start + initrd_size + 0xFFF) & ~0xFFF;
        mm_phys_reserve("Initrd", &phys_reserve_initrd);
    }

    amd64_phys_memory_map(&phys_memory_map);

    amd64_gdt_init();
    amd64_idt_init(0);

    amd64_mm_init();

    vesa_add_display();

    if (elf_sections.kind != KSYM_TABLE_NONE) {
        ksym_set(&elf_sections);
    }

    if (rs232_avail & (1 << 0)) {
        rs232_add_tty(0);
    }

    console_init_default();

    ps2_register_device();

    amd64_acpi_init();

    // Print kernel version now
    kinfo("yggdrasil " KERNEL_VERSION_STR "\n");

    //if (!multiboot_tag_sections) {
    //    kwarn("No ELF sections provided, module loading is unavailable\n");
    //}

    amd64_apic_init();
    rtc_init();
    // Setup system time
    struct tm t;
    rtc_read(&t);
    system_boot_time = mktime(&t);
    kinfo("Boot time: %04u-%02u-%02u %02u:%02u:%02u\n",
        t.tm_year, t.tm_mon, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec);

    if (initrd_phys_start) {
        // Create ram0 block device
        ramblk_init(MM_VIRTUALIZE(initrd_phys_start), initrd_size);
    }

    amd64_make_random_seed();

    amd64_fpu_init();

#if defined(AMD64_SMP)
    amd64_smp_init();
#endif
}

void kernel_main(uint64_t entry_method) {
    kernel_early_init(entry_method);
    main();
}
