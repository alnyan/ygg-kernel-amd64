#include "sys/mem/phys.h"
#include "sys/mem/slab.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/list.h"
#include "sys/attr.h"
#include "sys/mm.h"

typedef uint32_t bufctl_t;
#define BUFCTL_END      ((bufctl_t) -1)
#define slab_bufctl(slabp) \
    ((bufctl_t *) (((struct slab *) (slabp)) + 1))

#define CACHE_SIZE(i) \
    { 1ULL << ((i) + 4), &predefined_caches[i] }
#define CACHE_SIZE_END \
    { 0, NULL }
#define PREALLOC_COUNT  6

struct slab_cache {
    struct list_head slabs_empty, slabs_partial, slabs_full;
    size_t object_size, objects_per_slab;
};

struct slab {
    struct list_head list;
    size_t inuse;
    void *base;
    bufctl_t free;
};

struct cache_size {
    size_t size;
    struct slab_cache *cp;
};

struct slab_cache predefined_caches[PREALLOC_COUNT];
struct cache_size predefined_cache_sizes[PREALLOC_COUNT + 1] = {
    CACHE_SIZE(0),
    CACHE_SIZE(1),
    CACHE_SIZE(2),
    CACHE_SIZE(3),
    CACHE_SIZE(4),
    CACHE_SIZE(5),
    CACHE_SIZE_END
};

static void slab_init_cache(struct slab_cache *cp, size_t object_size) {
    cp->object_size = object_size;
    list_head_init(&cp->slabs_empty);
    list_head_init(&cp->slabs_partial);
    list_head_init(&cp->slabs_full);

    // Reserve space for slab descriptor
    cp->objects_per_slab = (MM_PAGE_SIZE - sizeof(struct slab)) / cp->object_size;
    // Reserve space for bufctl
    size_t bufctl_array_size = cp->objects_per_slab * sizeof(bufctl_t);
    cp->objects_per_slab -= (bufctl_array_size + cp->object_size - 1) / cp->object_size;
}

__init(slab_init_caches) {
    for (size_t i = 0; i < PREALLOC_COUNT; ++i) {
        kdebug("Initializing predefined cache: %u\n", 1UL << (i + 4));
        slab_init_cache(&predefined_caches[i], 1UL << (i + 4));
    }
}

struct slab_cache *slab_cache_get(size_t size) {
    struct cache_size *sizep = predefined_cache_sizes;
    for (; sizep->size; ++sizep) {
        if (size > sizep->size) {
            continue;
        }
        return sizep->cp;
    }

    panic("Tried to get cache of invalid size: %u\n", size);
}

////

static struct slab *slab_create(struct slab_cache *cp) {
    uintptr_t page_phys = mm_phys_alloc_page();

    if (page_phys == MM_NADDR) {
        return NULL;
    }

    struct slab *slab = (struct slab *) MM_VIRTUALIZE(page_phys);

    slab->base = ((void *) &slab[1]) + sizeof(bufctl_t) * cp->objects_per_slab;
    slab->base = (void *) (((uintptr_t) slab->base + 0x7) & ~0x7);
    list_head_init(&slab->list);
    slab->inuse = 0;
    slab->free = 0;

    for (size_t i = 0; i < cp->objects_per_slab - 1; ++i) {
        slab_bufctl(slab)[i] = i + 1;
    }
    slab_bufctl(slab)[cp->objects_per_slab - 1] = BUFCTL_END;
    list_add(&slab->list, &cp->slabs_empty);

    kdebug("Created slab of %u x %u B\n", cp->objects_per_slab, cp->object_size);

    return slab;
}

static void slab_destroy(struct slab_cache *cp, struct slab *slabp) {
    _assert(!((uintptr_t) slabp & 0xFFF));
    kdebug("Destoyed slab of %u x %u B\n", cp->objects_per_slab, cp->object_size);
    mm_phys_free_page(MM_PHYS(slabp));
}

static inline void *slab_alloc_from(struct slab_cache *cp, struct slab *slabp) {
    void *objp;
    ++slabp->inuse;
    // Get object based on "free" index
    objp = slabp->base + slabp->free * cp->object_size;
    // Set next "free" index
    slabp->free = slab_bufctl(slabp)[slabp->free];

    // All the pages are allocated
    if (slabp->free == BUFCTL_END) {
        list_del_init(&slabp->list);
        list_add(&slabp->list, &cp->slabs_full);
    }

    memset(objp, 0, cp->object_size);
    return objp;
}

void *slab_calloc_int(struct slab_cache *cp) {
    struct list_head *slabs_partial, *slabs_empty, *entry;
    struct slab *slabp;
try_again:

    // Check if there is any partial slab to use
    slabs_partial = &cp->slabs_partial;
    entry = slabs_partial->next;
    if (slabs_partial == entry) {
        // No partial slabs, try to find an empty one

        slabs_empty = &cp->slabs_empty;
        entry = slabs_empty->next;
        if (slabs_empty == entry) {
            // Have to allocate a new slab - no free objects are available
            // After that, just try allocation again
            //printf("No partial/empty slabs, allocating a new one\n");

            if (!slab_create(cp)) {
                return NULL;
            }
            goto try_again;
        }

        list_del(entry);
        list_add(entry, slabs_partial);
    }

    slabp = list_entry(entry, struct slab, list);
    return slab_alloc_from(cp, slabp);
}

void slab_free_int(struct slab_cache *cp, void *objp) {
    struct slab *slabp;
    // Get page-aligned address
    uintptr_t page = (uintptr_t) objp;
    _assert(page & 0xFFF);
    page /= MM_PAGE_SIZE;
    page *= MM_PAGE_SIZE;

    // slab descriptor is on the same page objp points to
    slabp = (struct slab *) page;

    unsigned int obj_number = (objp - slabp->base) / cp->object_size;
    slab_bufctl(slabp)[obj_number] = slabp->free;
    slabp->free = obj_number;

    size_t inuse = slabp->inuse;

    if (!--slabp->inuse) {
        // Was partial or full, now empty
        // Free the slab
        list_del(&slabp->list);
        slab_destroy(cp, slabp);
    } else if (inuse == cp->objects_per_slab) {
        // Was full, now partial
        list_del_init(&slabp->list);
        list_add(&slabp->list, &cp->slabs_partial);
    }
}

void slab_stat(struct slab_stat *st) {
    st->alloc_bytes = 0;
    st->alloc_objects = 0;
    st->alloc_pages = 0;

    for (size_t i = 0; i < PREALLOC_COUNT; ++i) {
        struct slab_cache *cp = &predefined_caches[i];
        struct slab *slab;

        list_for_each_entry(slab, &cp->slabs_full, list) {
            _assert(slab->inuse == cp->objects_per_slab);
            st->alloc_objects += slab->inuse;
            st->alloc_bytes += slab->inuse * cp->object_size;
            ++st->alloc_pages;
        }

        list_for_each_entry(slab, &cp->slabs_partial, list) {
            _assert(slab->inuse && slab->inuse != cp->objects_per_slab);
            st->alloc_objects += slab->inuse;
            st->alloc_bytes += slab->inuse * cp->object_size;
            ++st->alloc_pages;
        }

        list_for_each_entry(slab, &cp->slabs_empty, list) {
            panic("This list should be empty, slabs are freed once they're empty\n");
        }
    }
}

// Tracing
#if defined(SLAB_TRACE_ALLOC)

void *slab_calloc_trace(const char *filename, int line, struct slab_cache *cp) {
    void *objp = slab_calloc_int(cp);
    debugf(DEBUG_DEFAULT, "\033[43;30m%s:%d: slab allocate %u = %p\033[0m\n", filename, line, cp->object_size, objp);
    return objp;
}

void slab_free_trace(const char *filename, int line, struct slab_cache *cp, void *ptr) {
    debugf(DEBUG_DEFAULT, "\033[44;32m%s:%d: slab free     %p (%u)\033[0m\n", filename, line, ptr, cp->object_size);
    slab_free_int(cp, ptr);
}

#endif
