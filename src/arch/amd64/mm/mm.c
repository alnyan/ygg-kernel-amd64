#include "sys/mem.h"
#include "sys/debug.h"
#include "sys/mm.h"

mm_space_t mm_kernel;

void amd64_mm_init(void) {
    kdebug("Memory manager init\n");

    // Physical memory at this point:
    //  0x000000 - 0x400000 - Used by kernel and info passed from the loader
    // Virtual memory:
    //  Lower 1GiB mapped to itself for loader access
    //  0xFFFFFF0000000000 (1GiB) is mapped to 0 for kernel access

    // XXX: assuming nothing important is there
    uint64_t *pml4 = (uint64_t *) (0x200000 - 0x1000);
    uint64_t *pdpt = (uint64_t *) (0x200000 - 0x2000);

    memset((void *) (0x200000 - 2 * 0x1000), 0, 0x1000 * 2);

    // 0xFFFFFF0000000000 -> 0 (1GiB) mapping
    pml4[AMD64_MM_MASK(KERNEL_VIRT_BASE) >> 39] = (uint64_t) pdpt | 1 | 2;
    pdpt[(AMD64_MM_MASK(KERNEL_VIRT_BASE) >> 30) & 0x1FF] = 1 | 2 | (1 << 7);

    // Load the new table
    asm volatile ("mov %0, %%cr3"::"a"(pml4):"memory");
}