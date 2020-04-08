#pragma once
#include "sys/types.h"

struct slab_stat {
    size_t alloc_bytes;
    size_t alloc_pages;
    size_t alloc_objects;
};

struct slab_cache;

struct slab_cache *slab_cache_get(size_t obj_size);

void slab_stat(struct slab_stat *st);
void *slab_calloc(struct slab_cache *cp);
void slab_free(struct slab_cache *cp, void *ptr);
