#include "sys/mm.h"
#include "sys/debug.h"
#include "arch/amd64/hw/gdt.h"

void kernel_main(void) {
    kdebug("Booting\n");

    // Memory management
    amd64_mm_init();
    amd64_gdt_init();
}
