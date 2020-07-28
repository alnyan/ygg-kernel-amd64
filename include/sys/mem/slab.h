#pragma once
#include <config.h>
#include "sys/types.h"

struct slab_stat {
    size_t alloc_bytes;
    size_t alloc_pages;
    size_t alloc_objects;
};

struct slab_cache;

struct slab_cache *slab_cache_get(size_t obj_size);

void slab_stat(struct slab_stat *st);

#if defined(SLAB_TRACE_ALLOC)
#define slab_calloc(cp)     slab_calloc_trace(__FILE__, __LINE__, cp)
#define slab_free(cp, ptr)  slab_free_trace(__FILE__, __LINE__, cp, ptr)

void *slab_calloc_trace(const char *filename, int line, struct slab_cache *cp);
void slab_free_trace(const char *filename, int line, struct slab_cache *cp, void *ptr);
#else
#define slab_calloc(cp)     slab_calloc_int(cp)
#define slab_free(cp, ptr)  slab_free_int(cp, ptr)
#endif

void *slab_calloc_int(struct slab_cache *cp);
void slab_free_int(struct slab_cache *cp, void *ptr);
