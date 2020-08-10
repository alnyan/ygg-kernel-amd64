#include <stddef.h>
#include "sys/assert.h"
#include "sys/string.h"
#include "user/errno.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/hash.h"
#include "sys/mem/slab.h"

#define HASH_CHECK_DUP

static struct slab_cache *shash_cache;

static size_t shash_hash(const void *_s) {
    const char *s = _s;
    size_t hash = 5381;
    int c;

    while ((c = *s++)) {
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}

static struct hash_pair *shash_pair_new(void *key, void *value) {
    // Must fit into 128-sized block
    _assert(strlen(key) < 128 - sizeof(struct hash_pair));
    struct hash_pair *res;

    if (!shash_cache) {
        shash_cache = slab_cache_get(128);
        _assert(shash_cache);
    }

    res = slab_calloc(shash_cache);
    if (!res) {
        return NULL;
    }

    res->key = (void *) res + sizeof(struct hash_pair);
    res->value = value;
    strcpy((void *) res + sizeof(struct hash_pair), key);
    list_head_init(&res->link);

    return res;
}

static void shash_pair_free(struct hash_pair *pair) {
    _assert(shash_cache);
    slab_free(shash_cache, pair);
}

#if defined(HASH_DEBUG)
static void shash_pair_print(int level, struct hash_pair *pair) {
    debugf(level, "%s: %p\n", pair->key, pair->value);
}
#endif

int shash_init(struct hash *h, size_t cap) {
    h->hash = shash_hash;
    h->pair_new = shash_pair_new;
    h->pair_free = shash_pair_free;
    h->keycmp = (int (*) (const void *, const void *)) strcmp;
#if defined(HASH_DEBUG)
    h->pair_print = shash_pair_print;
#endif

    h->bucket_count = cap;
    h->buckets = kmalloc(sizeof(struct list_head) * cap);

    if (!h->buckets) {
        return -ENOMEM;
    }

    for (size_t i = 0; i < cap; ++i) {
        list_head_init(&h->buckets[i]);
    }

    return 0;
}

int hash_insert(struct hash *h, const void *key, void *value) {
#if defined(HASH_CHECK_DUP)
    _assert(!hash_lookup(h, key));
#endif
    struct hash_pair *pair = h->pair_new((void *) key, value);
    if (!pair) {
        return -ENOMEM;
    }
    size_t index = h->hash(key) % h->bucket_count;
    list_add(&pair->link, &h->buckets[index]);
    return 0;
}

struct hash_pair *hash_lookup(struct hash *h, const void *key) {
    size_t index = h->hash(key) % h->bucket_count;
    struct hash_pair *pair;

    list_for_each_entry(pair, &h->buckets[index], link) {
        if (!h->keycmp(pair->key, key)) {
            return pair;
        }
    }
    return NULL;
}

#if defined(HASH_DEBUG)
void hash_dump(struct hash *h) {
    struct hash_pair *pair;
    for (size_t i = 0; i < h->bucket_count; ++i) {
        list_for_each_entry(pair, &h->buckets[i], link) {
            h->pair_print(DEBUG_DEFAULT, pair);
        }
    }
}
#endif
