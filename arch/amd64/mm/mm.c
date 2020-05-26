#include "arch/amd64/cpuid.h"
#include "arch/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "arch/amd64/mm/mm.h"
#include "sys/panic.h"
#include "sys/heap.h"
#include "arch/amd64/mm/pool.h"
#include "arch/amd64/mm/phys.h"
#include "sys/mem/phys.h"
#include "sys/mm.h"

mm_space_t mm_kernel;

extern int _kernel_end;

void userptr_check(const void *ptr) {
    // TODO: "hardened" check - also check that the address is mapped
    assert(ptr, "invalid userptr: NULL\n");
    assert((uintptr_t) ptr < KERNEL_VIRT_BASE, "invalid userptr: in kernel space (%p)\n", ptr);
}

void amd64_mm_init(void) {
    kdebug("Memory manager init\n");

    // TODO: restore NX and do it per-CPU
    //{
    //    // TODO: make NX optional via config
    //    if (!(cpuid_ext_features_edx & CPUID_EXT_EDX_FEATURE_NX)) {
    //        panic("NX is not supported\n");
    //    }

    //    uint64_t efer = rdmsr(MSR_IA32_EFER);
    //    efer |= IA32_EFER_NXE;
    //    wrmsr(MSR_IA32_EFER, efer);
    //}

    mm_kernel = (mm_space_t) (MM_VIRTUALIZE(0x1FF000));
    // Create a pool located right after kernel image
    amd64_mm_pool_init((uintptr_t) &_kernel_end, MM_POOL_SIZE);

    uintptr_t heap_base_phys = mm_phys_alloc_contiguous(KERNEL_HEAP >> 12);
    assert(heap_base_phys != MM_NADDR, "Could not allocate %S of memory for kernel heap\n", KERNEL_HEAP);
    kdebug("Setting up kernel heap of %S @ %p\n", KERNEL_HEAP, heap_base_phys);
    amd64_heap_init(heap_global, heap_base_phys, KERNEL_HEAP);

    amd64_heap_dump(heap_global);
}
