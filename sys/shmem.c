#include "arch/amd64/mm/phys.h"
#include "arch/amd64/mm/map.h"
#include "arch/amd64/cpu.h"
#include "sys/block/blk.h"
#include "sys/vmalloc.h"
#include "user/errno.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/shmem.h"
#include "user/mman.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"
#include "fs/node.h"
#include "sys/mm.h"

static spin_t g_shm_lock = 0;
static LIST_HEAD(g_shm_region_head);

struct shm_region *shm_region_create(size_t count) {
    kdebug("Allocating a SHM region of %u pages\n", count);
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

    kdebug("Releasing %u pages\n", region->page_count);
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

    reg_ref->type = SHM_TYPE_PHYS;
    reg_ref->virt = vaddr;
    reg_ref->map.region = region;
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
        switch (ref->type) {
        case SHM_TYPE_PHYS:
            _assert(ref->map.region);

            if (addr >= ref->virt && addr <= ref->virt + ref->map.region->page_count * MM_PAGE_SIZE) {
                return ref;
            }
            break;
        case SHM_TYPE_BLOCK:
            if (addr >= ref->virt && addr <= ref->virt + ref->map.block.page_count * MM_PAGE_SIZE) {
                return ref;
            }
            break;
        }
    }
    return NULL;
}

void *sys_mmap(void *hint, size_t length, int prot, int flags, int fd, off_t off) {
    uintptr_t irq;
    size_t page_count;
    struct shm_region *shm;
    struct thread *thr;

    thr = thread_self;
    _assert(thr);

    if (length & MM_PAGE_OFFSET_MASK) {
        return (void *) -EINVAL;
    }
    page_count = length / MM_PAGE_SIZE;

    if (flags & MAP_ANONYMOUS) {
        kdebug("Anonymous mapping requested\n");
        if ((flags & MAP_ACCESS_MASK) != MAP_PRIVATE) {
            kwarn("Can't have shared anonymous mapping\n");
            return (void *) -EINVAL;
        }

        shm = shm_region_create(page_count);
        _assert(shm);

        spin_lock_irqsave(&g_shm_lock, &irq);
        list_add(&shm->link, &g_shm_region_head);
        spin_release_irqrestore(&g_shm_lock, &irq);

        uintptr_t addr = shm_region_map(shm, thr);

        kdebug("New region mapping list:\n");
        shm_thread_describe(thr);

        return addr == MM_NADDR ? (void *) -ENOMEM : (void *) addr;
    } else {
        kdebug("File mapping requested\n");

        if (fd < 0 || fd >= THREAD_MAX_FDS) {
            return (void *) -EBADF;
        }

        struct ofile *of = thr->fds[fd];
        if (!of) {
            return (void *) -EBADF;
        }

        if (of->flags & OF_SOCKET) {
            kwarn("%d tried to mmap a socket\n", thr->pid);
            return (void *) -EINVAL;
        }

        struct vnode *vn = of->file.vnode;
        _assert(vn);

        if (vn->type != VN_BLK) {
            kwarn("%d tried to mmap a non-block file\n", thr->pid);
            return (void *) -ENOTBLK;
        }

        struct blkdev *blk = vn->dev;
        _assert(blk);

        if (!blk->mmap) {
            kwarn("%d tried to mmap block device, but no function present\n", thr->pid);
            return (void *) -ENOENT;
        }

        struct shm_region_ref *reg_ref = kmalloc(sizeof(struct shm_region_ref));
        _assert(reg_ref);
        void *vaddr = blk->mmap(blk, of, hint, length, flags);

        if (vaddr == NULL || vaddr == MAP_FAILED) {
            kfree(reg_ref);
            return MAP_FAILED;
        }

        reg_ref->type = SHM_TYPE_BLOCK;
        reg_ref->virt = (uintptr_t) vaddr;
        reg_ref->map.block.blk = blk;
        reg_ref->map.block.page_count = length / MM_PAGE_SIZE;
        kinfo("Created a mapping of %u pages at %p\n", length / MM_PAGE_SIZE, vaddr);

        list_add(&reg_ref->link, &thr->shm_list);

        return vaddr;
    }
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
    switch (reg_ref->type) {
    case SHM_TYPE_PHYS: {
            struct shm_owner_ref *own_ref = NULL, *own_it;
            if (!reg_ref) {
                // No region is mapped there
                return -ENOENT;
            }
            struct shm_region *reg = reg_ref->map.region;
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
            list_del(&own_ref->link);
            --reg->ref_count;

            if (!reg->ref_count) {
                spin_release_irqrestore(&reg->lock, &irq);
                shm_region_free(reg);
            } else {
                spin_release_irqrestore(&reg->lock, &irq);
            }

            kfree(own_ref);
        }
        break;
    case SHM_TYPE_BLOCK: {
            struct blkdev *blk = reg_ref->map.block.blk;
            _assert(blk);

            kwarn("TODO: block device munmap call\n");

            // Unmap pages
            for (size_t i = 0; i < reg_ref->map.block.page_count; ++i) {
                _assert(amd64_map_umap(thr->space, reg_ref->virt + i * MM_PAGE_SIZE, 1) != MM_NADDR);
            }
            kdebug("Unmapped %u pages at %p\n", reg_ref->map.block.page_count, reg_ref->virt);
        }
        break;
    }
    list_del(&reg_ref->link);
    kfree(reg_ref);

    kdebug("New region mapping list:\n");
    shm_thread_describe(thr);

    return 0;
}

void shm_thread_describe(struct thread *thr) {
    struct shm_region_ref *ref;
    list_for_each_entry(ref, &thr->shm_list, link) {
        switch (ref->type) {
        case SHM_TYPE_PHYS: {
                struct shm_region *reg = ref->map.region;
                kdebug(" - %3u pages - %p\n", reg->page_count, ref->virt);
                for (size_t i = 0; i < reg->page_count; ++i) {
                    kdebug("     - %p\n", reg->phys[i]);
                }
            }
            break;
        case SHM_TYPE_BLOCK:
            break;
        }
    }
}

void shm_space_fork(struct thread *dst, struct thread *src) {
    kdebug("Forkng shared memory regions\n");
    struct shm_region_ref *ref;
    list_for_each_entry(ref, &src->shm_list, link) {
        switch (ref->type) {
        case SHM_TYPE_PHYS: {
                struct shm_region *reg = ref->map.region;
                kdebug(" Fork %3u pages - %p\n", reg->page_count, ref->virt);

                shm_region_map(reg, dst);
            }
            break;
        case SHM_TYPE_BLOCK:
            break;
        }
    }
}

void shm_region_release_all(struct thread *thr, int noumap) {
    struct shm_region_ref *reg_ref;
    struct shm_owner_ref *own_ref, *own_it;
    struct shm_region *region;
    uintptr_t irq;

    list_for_each_entry(reg_ref, &thr->shm_list, link) {
        switch (reg_ref->type) {
        case SHM_TYPE_PHYS:
            region = reg_ref->map.region;
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
            break;

        case SHM_TYPE_BLOCK: {
                struct blkdev *blk = reg_ref->map.block.blk;
                _assert(blk);

                kwarn("TODO: block device munmap call\n");

                if (!noumap) {
                    // Unmap pages
                    for (size_t i = 0; i < reg_ref->map.block.page_count; ++i) {
                        _assert(amd64_map_umap(thr->space, reg_ref->virt + i * MM_PAGE_SIZE, 1) != MM_NADDR);
                    }
                    kdebug("Unmapped %u pages at %p\n", reg_ref->map.block.page_count, reg_ref->virt);
                }
            }
            break;
        }
    }

    while (!list_empty(&thr->shm_list)) {
        list_del(thr->shm_list.next);
    }
}
