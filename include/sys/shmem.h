#pragma once
#include "sys/types.h"
#include "sys/list.h"
#include "sys/spin.h"

struct thread;
struct blkdev;

struct shm_region {
    spin_t lock;

    size_t ref_count;
    // List head for tracking ownership of the region
    struct list_head owners;
    // Link for tracking all the regions in the system
    struct list_head link;

    size_t page_count;
    uintptr_t phys[0];
};

// region -> thread ownership link
struct shm_owner_ref {
    struct thread *owner;
    uintptr_t virt;
    struct list_head link;
};

// thread -> region mapping link
struct shm_region_ref {
    enum {
        // -> memory pages
        SHM_TYPE_PHYS,
        // -> block device mapping
        SHM_TYPE_BLOCK,
    } type;
    uintptr_t virt;
    struct list_head link;
    union {
        // Physical mapping
        struct shm_region *region;
        // Block device mapping
        struct {
            size_t page_count;
            uintptr_t phys_base;
            struct blkdev *blk;
        } block;
    } map;
};

// Setup an empty shared memory region of N pages
struct shm_region *shm_region_create(size_t count);
// Destroy an empty region
void shm_region_free(struct shm_region *region);

// Map a shared physical page region into
// thread's virtial memory space
uintptr_t shm_region_map(struct shm_region *region, struct thread *thr);
// Release all shared memory regions of the specified thread
// If noumap != 0, then thread's page structures are assumed
// to be non-existent and no virtual unmaps are performed
void shm_region_release_all(struct thread *thr, int noumap);
// Find a shared memory region by its virtual address
struct shm_region_ref *shm_region_find(struct thread *thr, uintptr_t ptr);

void shm_space_fork(struct thread *dst, struct thread *src);

void shm_region_describe(struct shm_region *region);
void shm_thread_describe(struct thread *thr);

void *sys_mmap(void *hint, size_t length, int prot, int flags, int fd, off_t offset);
int sys_munmap(void *addr, size_t length);