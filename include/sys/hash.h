#pragma once
#include "sys/types.h"
#include "sys/list.h"

struct hash_pair {
    void *key, *value;
    struct list_head link;
};

#define HASH_DEBUG

struct hash {
    size_t (*hash) (const void *);
    struct hash_pair *(*pair_new) (void *, void *);
    void (*pair_free) (struct hash_pair *);
    int (*keycmp) (const void *, const void *);
#if defined(HASH_DEBUG)
    void (*pair_print) (int, struct hash_pair *);
#endif

    size_t bucket_count;
    struct list_head *buckets;
};

int shash_init(struct hash *h, size_t cap);
int hash_insert(struct hash *h, const void *key, void *value);
struct hash_pair *hash_lookup(struct hash *h, const void *key);

#if defined(HASH_DEBUG)
void hash_dump(struct hash *h);
#else
#define hash_dump(h)
#endif
