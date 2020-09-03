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

//static struct multiboot_tag_mmap            *multiboot_tag_mmap;
//static struct multiboot_tag_elf_sections    *multiboot_tag_sections;
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
        //case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
        //    multiboot_tag_sections = (struct multiboot_tag_elf_sections *) tag;
        //    break;
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

// TODO: move this header
#define BOOT_KERNEL_MAGIC           0xA197A9B007B007UL
#define BOOT_LOADER_MAGIC           0x700B700B9A791AUL

#define VIDEO_FORMAT_RGB32          0
#define VIDEO_FORMAT_BGR32          1

#if !defined(__ASM__)
struct boot_struct {
    uint64_t kernel_magic;          // R
    uint64_t loader_magic;          // W

    uint64_t memory_map_base;       // W
    uint32_t memory_map_size;       // W
    uint32_t memory_map_entsize;    // W

    // Video mode settings
    uint32_t video_width;           // RW
    uint32_t video_height;          // RW
    uint32_t video_format;          // RW
    uint32_t __pad0;                // --
    uint64_t video_framebuffer;     // W
    uint64_t video_pitch;           // W

    uint64_t elf_symtab_hdr;        // W
    uint64_t elf_symtab_data;       // W
    uint64_t elf_strtab_hdr;        // W
    uint64_t elf_strtab_data;       // W

    uint64_t initrd_base;           // W
    uint64_t initrd_size;           // W

    uint64_t rsdp;                  // W

    char cmdline[256];              // W
} __attribute__((packed));
#endif

static void entry_yboot(void) {
    extern struct boot_struct yboot_data;

    if (yboot_data.memory_map_base == 0) {
        panic("No memory map available\n");
    }

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

    phys_memory_map.format = MM_PHYS_MMAP_FMT_YBOOT;
    phys_memory_map.address = (void *) MM_VIRTUALIZE(yboot_data.memory_map_base);
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

    // Reinitialize RS232 properly
    rs232_init(RS232_COM1);
    ps2_init();

    // Before anything is allocated, reserve:
    // 1. initrd pages
    // 2. multiboot tag pages
    mm_phys_reserve(&phys_reserve_kernel);

    if (initrd_phys_start) {
        phys_reserve_initrd.begin = initrd_phys_start & ~0xFFF;
        phys_reserve_initrd.end = (initrd_phys_start + initrd_size + 0xFFF) & ~0xFFF;
        mm_phys_reserve(&phys_reserve_initrd);
    }

    amd64_phys_memory_map(&phys_memory_map);

    amd64_gdt_init();
    amd64_idt_init(0);

    amd64_mm_init();

    vesa_init(&boot_video_info);

    //if (multiboot_tag_sections) {
    //    ksym_set_multiboot2(multiboot_tag_sections);
    //}

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
