#include "arch/amd64/mm/phys.h"
#include "arch/amd64/mm/map.h"
#include "arch/amd64/cpu.h"
#include "sys/vmalloc.h"
#include "user/errno.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/shmem.h"
#include "user/mman.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/mm.h"

static spin_t mman_lock = 0;
static struct shm_region *mman_region = NULL;

struct shm_region *shm_region_create(size_t count) {
    kinfo("Allocating a SHM region of %u pages\n", count);
    struct shm_region *res = kmalloc(sizeof(struct shm_region) + count * sizeof(uintptr_t));
    _assert(res);

    for (size_t i = 0; i < count; ++i) {
        res->phys[i] = amd64_phys_alloc_page();
        _assert(res->phys[i] != MM_NADDR);
    }

    res->page_count = count;
    res->ref_count = 0;
    list_head_init(&res->owners);
    res->lock = 0;

    return res;
}

void shm_region_free(struct shm_region *region) {
    uintptr_t irq;
    spin_lock_irqsave(&region->lock, &irq);
    _assert(list_empty(&region->owners));
    _assert(!region->ref_count);

    kinfo("Releasing %u pages\n", region->page_count);
    for (size_t i = 0; i < region->page_count; ++i) {
        amd64_phys_free(region->phys[i]);
    }

    spin_release_irqrestore(&region->lock, &irq);
    kfree(region);
}

uintptr_t shm_region_map(struct shm_region *region, struct thread *thr) {
    uintptr_t irq;
    spin_lock_irqsave(&region->lock, &irq);
    kdebug("Mapping region of %u pages into thread %d\n", region->page_count, thr->pid);

    struct shm_region_ref *reg_ref = kmalloc(sizeof(struct shm_region_ref));
    _assert(reg_ref);
    struct shm_owner_ref *own_ref = kmalloc(sizeof(struct shm_owner_ref));
    _assert(own_ref);

    uintptr_t vaddr = vmfind(thr->space, 0x80000000, 0x100000000, region->page_count);

    kdebug("-> %p\n", vaddr);

    reg_ref->virt = vaddr;
    reg_ref->region = region;
    list_head_init(&reg_ref->link);
    own_ref->virt = vaddr;
    own_ref->owner = thr;
    list_head_init(&own_ref->link);

    // Map all the pages
    for (size_t i = 0; i < region->page_count; ++i) {
        if (amd64_map_single(thr->space,
                             vaddr + i * MM_PAGE_SIZE,
                             region->phys[i],
                             MM_PAGE_USER | MM_PAGE_WRITE) != 0) {
            panic("map failed\n");
        }
    }

    // Add region ref to thread
    list_add(&reg_ref->link, &thr->shm_list);
    // Add owner ref to region
    list_add(&own_ref->link, &region->owners);

    ++region->ref_count;

    spin_release_irqrestore(&region->lock, &irq);

    return vaddr;
}

// Slow, but kinda works
struct shm_region_ref *shm_region_find(struct thread *thr, uintptr_t addr) {
    struct shm_region_ref *ref;
    list_for_each_entry(ref, &thr->shm_list, link) {
        struct shm_region *reg = ref->region;
        _assert(reg);

        if (addr >= ref->virt && addr <= ref->virt + reg->page_count * MM_PAGE_SIZE) {
            return ref;
        }
    }
    return NULL;
}

void *sys_mmap(void *hint, size_t length, int prot, int flags, int fd, off_t off) {
    uintptr_t irq;
    struct shm_region *shm;
    struct thread *thr;

    thr = thread_self;
    _assert(thr);

    spin_lock_irqsave(&mman_lock, &irq);
    if (!mman_region) {
        mman_region = shm_region_create(length / 0x1000);
    }
    _assert(mman_region);

    shm = mman_region;
    spin_release_irqrestore(&mman_lock, &irq);

    uintptr_t addr = shm_region_map(shm, thr);

    kdebug("New region mapping list:\n");
    shm_thread_describe(thr);

    return addr == MM_NADDR ? MAP_FAILED : (void *) addr;
}

int sys_munmap(void *_addr, size_t length) {
    uintptr_t addr = (uintptr_t) _addr;
    uintptr_t irq;
    struct thread *thr;

    if (addr & MM_PAGE_OFFSET_MASK) {
        // Address is not page-aligned
        return -EINVAL;
    }

    thr = thread_self;
    _assert(thr);

    struct shm_region_ref *reg_ref = shm_region_find(thr, addr);
    struct shm_owner_ref *own_ref = NULL, *own_it;
    if (!reg_ref) {
        // No region is mapped there
        return -ENOENT;
    }
    struct shm_region *reg = reg_ref->region;
    _assert(reg);

    // Find region -> thread owner reference
    list_for_each_entry(own_it, &reg->owners, link) {
        if (own_it->virt == reg_ref->virt && own_it->owner == thr) {
            own_ref = own_it;
            break;
        }
    }
    _assert(own_ref);

    // Unmap pages
    for (size_t i = 0; i < reg->page_count; ++i) {
        _assert(amd64_map_umap(thr->space, reg_ref->virt + i * MM_PAGE_SIZE, 1) != MM_NADDR);
    }

    spin_lock_irqsave(&reg->lock, &irq);
    _assert(reg->ref_count);
    list_del(&reg_ref->link);
    list_del(&own_ref->link);
    --reg->ref_count;

    if (!reg->ref_count) {
        spin_release_irqrestore(&reg->lock, &irq);
        shm_region_free(reg);
    } else {
        spin_release_irqrestore(&reg->lock, &irq);
    }

    kfree(own_ref);
    kfree(reg_ref);

    kdebug("New region mapping list:\n");
    shm_thread_describe(thr);

    return 0;
}

void shm_thread_describe(struct thread *thr) {
    struct shm_region_ref *ref;
    list_for_each_entry(ref, &thr->shm_list, link) {
        struct shm_region *reg = ref->region;
        kdebug(" - %3u pages - %p\n", reg->page_count, ref->virt);
        for (size_t i = 0; i < reg->page_count; ++i) {
            kdebug("     - %p\n", reg->phys[i]);
        }
    }
}

void shm_space_fork(struct thread *dst, struct thread *src) {
    kdebug("Forkng shared memory regions\n");
    struct shm_region_ref *ref;
    list_for_each_entry(ref, &src->shm_list, link) {
        struct shm_region *reg = ref->region;
        kdebug(" Fork %3u pages - %p\n", reg->page_count, ref->virt);

        shm_region_map(reg, dst);
    }
}

void shm_region_release_all(struct thread *thr, int noumap) {
    struct shm_region_ref *reg_ref;
    struct shm_owner_ref *own_ref, *own_it;
    struct shm_region *region;
    uintptr_t irq;

    list_for_each_entry(reg_ref, &thr->shm_list, link) {
        region = reg_ref->region;
        own_ref = NULL;
        _assert(region);

        kdebug("Releasing %u pages at %p\n", region->page_count, reg_ref->virt);

        list_for_each_entry(own_it, &region->owners, link) {
            if (own_it->virt == reg_ref->virt && own_it->owner == thr) {
                own_ref = own_it;
                break;
            }
        }
        _assert(own_ref);

        // Unmap pages
        if (!noumap) {
            for (size_t i = 0; i < region->page_count; ++i) {
                _assert(amd64_map_umap(thr->space, reg_ref->virt + i * MM_PAGE_SIZE, 1) != MM_NADDR);
            }
        }

        spin_lock_irqsave(&region->lock, &irq);
        _assert(region->ref_count);
        list_del(&own_ref->link);
        --region->ref_count;

        if (!region->ref_count) {
            spin_release_irqrestore(&region->lock, &irq);
            shm_region_free(region);
        } else {
            spin_release_irqrestore(&region->lock, &irq);
        }

        kfree(own_ref);
    }

    while (!list_empty(&thr->shm_list)) {
        list_del(thr->shm_list.next);
    }
}
