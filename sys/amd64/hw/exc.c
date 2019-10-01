#include "sys/amd64/hw/ints.h"
#include "sys/amd64/mm/map.h"
#include "sys/amd64/regs.h"
#include "sys/debug.h"

uint64_t amd64_err_num, amd64_err_code;

void amd64_exc_handler(amd64_ctx_regs_t *regs) {
    kfatal("Unhandled exception #%d\n", amd64_err_num);

    amd64_ctx_dump(DEBUG_FATAL, regs);

    if (amd64_err_num == 14) {
        mm_space_t pml4 = (mm_space_t) MM_VIRTUALIZE(regs->cr3);

        kfatal("This is a page fault\n");
        uintptr_t cr2;
        asm volatile ("movq %%cr2, %0":"=a"(cr2));
        kfatal("cr2 = %p\n", cr2);

        uint64_t flags;
        uintptr_t paddr = amd64_map_get(pml4, cr2, &flags);

        if (paddr == MM_NADDR) {
            kfatal("Requested page is not present\n");
        } else {
            kfatal("Requested page physical address is %p\n", paddr);
        }

        kfatal("Error code: %p\n", amd64_err_code);
        if (amd64_err_code & (1 << 0)) {
            kfatal("[Present]\n");
        }
        if (amd64_err_code & (1 << 1)) {
            kfatal("[Write]\n");
        }
        if (amd64_err_code & (1 << 2)) {
            kfatal("[User]\n");
            kfatal("Page \"user\" flag is %d\n", !!(flags & (1 << 2)));
        }
        if (amd64_err_code & (1 << 3)) {
            kfatal("[Reserved Write]\n");
        }
        if (amd64_err_code & (1 << 4)) {
            kfatal("[Instruction Fetch]\n");
        }
    }

    while (1) {
        asm volatile ("cli; hlt");
    }
}
