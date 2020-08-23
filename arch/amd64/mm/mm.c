#include "arch/amd64/cpuid.h"
#include "arch/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "arch/amd64/mm/mm.h"
#include "sys/panic.h"
#include "sys/heap.h"
#include "arch/amd64/mm/phys.h"
#include "sys/mem/phys.h"
#include "sys/mm.h"

mm_space_t mm_kernel;

// Reserved space for kernel page structs
// TODO: option to enable PDPE1GB
// 1x PML4; 1x PDPT; 4x PD
__attribute__((aligned(0x1000))) uint64_t kernel_pd_res[6 * 512];

extern int _kernel_end;

void userptr_check(const void *ptr) {
    // TODO: "hardened" check - also check that the address is mapped
    assert(ptr, "invalid userptr: NULL\n");
    assert((uintptr_t) ptr < KERNEL_VIRT_BASE, "invalid userptr: in kernel space (%p)\n", ptr);
}

void amd64_mm_init(void) {
    kdebug("Memory manager init\n");

    mm_kernel = &kernel_pd_res[5 * 512];

    uintptr_t heap_base_phys = mm_phys_alloc_contiguous(KERNEL_HEAP >> 12, PU_KERNEL);
    assert(heap_base_phys != MM_NADDR, "Could not allocate %S of memory for kernel heap\n", KERNEL_HEAP);
    kdebug("Setting up kernel heap of %S @ %p\n", KERNEL_HEAP, heap_base_phys);
    amd64_heap_init(heap_global, heap_base_phys, KERNEL_HEAP);

    amd64_heap_dump(heap_global);
}
