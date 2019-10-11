#include "sys/debug.h"
#include "sys/amd64/hw/gdt.h"
#include "sys/amd64/hw/idt.h"
#include "sys/amd64/mm/mm.h"
#include "sys/amd64/hw/acpi.h"

void kernel_main(struct amd64_loader_data *data) {
    amd64_gdt_init();
    amd64_idt_init();
    amd64_mm_init(NULL);

    extern void sched_init(void);
    sched_init();
    amd64_acpi_init();

    while (1) {
        asm ("sti; hlt");
    }
}
