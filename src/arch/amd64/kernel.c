#include "sys/mm.h"
#include "sys/debug.h"
#include "arch/amd64/hw/gdt.h"
#include "arch/amd64/hw/pic8259.h"
#include "arch/amd64/hw/ints.h"

// TODO: move to some util header
#define __wfe() asm volatile ("sti; hlt")

void kernel_main(void) {
    kdebug("Booting\n");

#if defined(KERNEL_TEST_MODE)
    kdebug("Kernel testing mode enabled\n");
#endif

    // Memory management
    amd64_mm_init();
    amd64_gdt_init();
    pic8259_init();
    amd64_idt_init();

#if defined(KERNEL_TEST_MODE)
    kdebug("mm_kernel after basic init\n");
    mm_describe(mm_kernel);
#endif

    while (1) {
        __wfe();
    }
}
