// #include "sys/vmalloc.h"
// #include "sys/assert.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "sys/amd64/mm/mm.h"
// #include "sys/panic.h"
// #include "sys/heap.h"
// #include "sys/mem.h"
// #include "sys/mm.h"

mm_space_t mm_kernel;

extern int _kernel_end;

void amd64_mm_init(struct amd64_loader_data *data) {
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

    // 0x0000000000000000 -> 0 Mapping for AP bootstrapping
    pml4[0] = ((uintptr_t) pdpt) | 1 | 2 | 4;
    // 0xFFFFFF0000000000 -> 0 (512GiB) mapping
    pml4[AMD64_MM_STRIPSX(KERNEL_VIRT_BASE) >> 39] = ((uintptr_t) pdpt) | 1 | 2 | 4;
    for (uint64_t i = 0; i < 4; ++i) {
        kdebug("Mapping %p -> %p\n", KERNEL_VIRT_BASE | (i << 30), i << 30);
        pdpt[((AMD64_MM_STRIPSX(KERNEL_VIRT_BASE) >> 30) + i) & 0x1FF] = (i << 30) | 1 | 2 | 4 | (1 << 7);
    }

    // Load the new table
    asm volatile ("mov %0, %%cr3"::"a"(pml4):"memory");

    // Create a pool located right after kernel image
    // amd64_mm_pool_init((uintptr_t) &_kernel_end, MM_POOL_SIZE);

    mm_kernel = (mm_space_t) (MM_VIRTUALIZE(pml4));

    // // Allocate some pages for kernel heap (base size: 16MiB)
    // uintptr_t heap_base_phys = amd64_phys_alloc_contiguous(KERNEL_HEAP >> 12);
    // assert(heap_base_phys != MM_NADDR, "Could not allocate %S of memory for kernel heap\n", KERNEL_HEAP);
    // kdebug("Setting up kernel heap of %S @ %p\n", KERNEL_HEAP, heap_base_phys);
    // amd64_heap_init(heap_global, heap_base_phys, KERNEL_HEAP);

    // amd64_heap_dump(heap_global);
}