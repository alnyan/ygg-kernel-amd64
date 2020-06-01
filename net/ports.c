#include "sys/mem/slab.h"
#include "sys/assert.h"
#include "net/ports.h"
#include <stddef.h>

static struct slab_cache *g_ent_cache = NULL;

void port_array_insert(struct port_array *arr, uint16_t key, void *value) {
    if (!g_ent_cache) {
        g_ent_cache = slab_cache_get(sizeof(struct port_array_entry));
        _assert(g_ent_cache);
    }

    size_t hash = key % PORT_ARRAY_BUCKET_COUNT;
    struct port_array_entry *ent;
    list_for_each_entry(ent, &arr->buckets[hash], list) {
        assert(ent->port_no != key, "Tried to insert a duplicate port mapping\n");
    }

    ent = slab_calloc(g_ent_cache);
    _assert(ent);

    ent->port_no = key;
    ent->socket = value;
    list_head_init(&ent->list);
    list_add(&ent->list, &arr->buckets[hash]);
}

void *port_array_delete(struct port_array *arr, uint16_t key) {
    size_t hash = key % PORT_ARRAY_BUCKET_COUNT;
    struct port_array_entry *ent;
    list_for_each_entry(ent, &arr->buckets[hash], list) {
        if (ent->port_no == key) {
            _assert(g_ent_cache);
            list_del(&ent->list);
            void *r = ent->socket;
            slab_free(g_ent_cache, ent);

            return r;
        }
    }

    return NULL;
}

int port_array_lookup(struct port_array *arr, uint16_t key, void **value) {
    size_t hash = key % PORT_ARRAY_BUCKET_COUNT;
    struct port_array_entry *ent;
    list_for_each_entry(ent, &arr->buckets[hash], list) {
        if (ent->port_no == key) {
            *value = ent->socket;
            return 0;
        }
    }

    return -1;
}

void port_array_init(struct port_array *arr) {
    for (size_t i = 0; i < PORT_ARRAY_BUCKET_COUNT; ++i) {
        list_head_init(&arr->buckets[i]);
    }
}
