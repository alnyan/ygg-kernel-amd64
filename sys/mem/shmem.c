#include "sys/mem/vmalloc.h"
#include "arch/amd64/cpu.h"
#include "sys/mem/shmem.h"
#include "sys/block/blk.h"
#include "sys/mem/phys.h"
#include "sys/mem/slab.h"
#include "user/errno.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "user/mman.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "fs/node.h"
#include "sys/mm.h"

static void *sys_mmap_anon(size_t page_count, int prot, int flags) {
    struct thread *thr;
    uint64_t map_flags = MM_PAGE_USER;
    int map_usage = PU_PRIVATE;

    if (prot & PROT_WRITE) {
        map_flags |= MM_PAGE_WRITE;
    }
    if (flags & MAP_SHARED) {
        map_usage = PU_SHARED;
    }

    thr = thread_self;
    _assert(thr);

    uintptr_t virt_base = vmfind(thr->space, 0x100000000, 0x400000000, page_count);
    _assert(virt_base != MM_NADDR);

    kdebug("mmaping %u pages at %p\n", page_count, virt_base);

    // Map pages
    for (size_t i = 0; i < page_count; ++i) {
        uintptr_t phys = mm_phys_alloc_page();
        _assert(phys != MM_NADDR);
        struct page *page = PHYS2PAGE(phys);
        _assert(page);

        page->flags |= PG_MMAPED;
        _assert(mm_map_single(thr->space, virt_base + i * MM_PAGE_SIZE, phys, map_flags, map_usage) == 0);
    }

    return (void *) virt_base;
}

void *sys_mmap(void *hint, size_t length, int prot, int flags, int fd, off_t off) {
    size_t page_count;

    if (length & MM_PAGE_OFFSET_MASK) {
        return (void *) -EINVAL;
    }
    page_count = length / MM_PAGE_SIZE;

    if (flags & MAP_ANONYMOUS) {
        // Anonymous mapping
        return sys_mmap_anon(page_count, prot, flags);
    } else {
        // File/device-backed mapping
        panic("TODO\n");
    }
}

int sys_munmap(void *ptr, size_t len) {
    uintptr_t addr = (uintptr_t) ptr;
    struct thread *thr;

    thr = thread_self;
    _assert(thr);

    if (addr & MM_PAGE_OFFSET_MASK) {
        kwarn("Misaligned address\n");
        return -EINVAL;
    }

    if (addr >= KERNEL_VIRT_BASE) {
        kwarn("Don't even try\n");
        return -EACCES;
    }

    if (len & MM_PAGE_OFFSET_MASK) {
        kwarn("Misaligned size\n");
        return -EINVAL;
    }

    len /= MM_PAGE_SIZE;

    for (size_t i = 0; i < len; ++i) {
        uint64_t flags;
        uintptr_t phys = mm_map_get(thr->space, addr + i * MM_PAGE_SIZE, &flags);

        if (phys == MM_NADDR) {
            continue;
        }

        struct page *page = PHYS2PAGE(phys);
        _assert(page);

        if (!(page->flags & PG_MMAPED)) {
            panic("Tried to unmap non-mmapped page\n");
        }

        _assert(page->refcount);
        _assert(mm_umap_single(thr->space, addr + i * MM_PAGE_SIZE, 1) == phys);

        if (page->usage == PU_SHARED || page->usage == PU_PRIVATE) {
            if (!page->refcount) {
                kdebug("Free page %p\n", phys);
                mm_phys_free_page(phys);
            }
        } else {
            panic("Unhandled page type: %d (%p -> %p)\n", page->usage, addr + i * MM_PAGE_SIZE, phys);
        }
    }

    return 0;
}
