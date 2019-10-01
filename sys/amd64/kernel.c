#include "sys/mm.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/hw/pic8259.h"
#include "sys/amd64/hw/ints.h"
#include "sys/amd64/hw/timer.h"
#include "sys/amd64/loader/data.h"
#include "sys/amd64/loader/multiboot.h"
#include "sys/amd64/mm/phys.h"
#include "sys/amd64/acpi/tables.h"
#include "sys/kidle.h"

// TODO: move to some util header
#define __wfe() asm volatile ("sti; hlt")

// For the sake of log readability
#define kernel_startup_section(text) \
    kdebug("\n"); \
    kdebug("====== " text "\n"); \
    kdebug("\n")

static struct amd64_loader_data *loader_data = 0;
static multiboot_info_t *multiboot_info;

static uint8_t amd64_loader_data_checksum(const struct amd64_loader_data *ld) {
    uint8_t chk = 0;
    for (size_t i = 0; i < sizeof(struct amd64_loader_data); ++i) {
        chk += ((uint8_t *) ld)[i];
    }
    return chk;
}

static void amd64_loader_data_process(void) {
    // Virtualize pointer locations, as the loader has passed their
    // physical locations
    multiboot_info = (multiboot_info_t *) MM_VIRTUALIZE(loader_data->multiboot_info_ptr);
}

void kernel_main(uintptr_t loader_info_phys_ptr) {
    kdebug("Booting\n");

#if defined(KERNEL_TEST_MODE)
    kdebug("Kernel testing mode enabled\n");
#endif

    // Obtain boot information from loader
    loader_data = (struct amd64_loader_data *) MM_VIRTUALIZE(loader_info_phys_ptr);
    if (amd64_loader_data_checksum(loader_data)) {
        panic("Loader data checksum is invalid\n");
    }
    // Process loader data
    amd64_loader_data_process();

    // Memory management
    kernel_startup_section("Memory management");
    amd64_phys_memory_map((multiboot_memory_map_t *) MM_VIRTUALIZE(multiboot_info->mmap_addr), multiboot_info->mmap_length);
    amd64_mm_init(loader_data);

    kernel_startup_section("Basic hardware");
    amd64_gdt_init();
    pic8259_init();
    acpi_tables_init();
    amd64_timer_configure();
    amd64_idt_init();

#if defined(KERNEL_TEST_MODE)
    kernel_startup_section("Test dumps");
    mm_describe(mm_kernel);
#endif

    extern void amd64_setup_syscall(void);
    amd64_setup_syscall();

    kidle_init();

    // Wait until entering [kidle]
    while (1) {
        __wfe();
    }
}
