#include "hash.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define k_malloc(n)     malloc(n)
#define k_free(p)       free(p)

int hash_u64_keycmp(uint64_t v0, uint64_t v1) {
    return !(v0 == v1);
}

uint64_t hash_u64_keyhsh(uint64_t v) {
    return v;
}

void hash_init(hash_t *h, int nb) {
    h->bucket_count = nb;
    h->item_count = 0;
    h->buckets = (hash_entry_t **) k_malloc(sizeof(hash_entry_t *) * nb);
    memset(h->buckets, 0, sizeof(hash_entry_t *) * nb);
}

void hash_put(hash_t *h, uint64_t key, void *v) {
    // Try to lookup the key first
    size_t index = h->keyhsh(key) % h->bucket_count;

    if (!h->buckets[index]) {
        hash_entry_t *e = (hash_entry_t *) k_malloc(sizeof(hash_entry_t));
        e->value = v;
        e->key = h->keydup ? h->keydup(key) : key;
        e->next = NULL;
        h->buckets[index] = e;
        ++h->item_count;
        return;
    }

    hash_entry_t *after = NULL;
    // Hash collision/match
    for (hash_entry_t *ent = h->buckets[index]; ent; ent = ent->next) {
        // Update the existing entry
        if (!h->keycmp(ent->key, key)) {
            if (h->valfree) {
                h->valfree(ent->value);
            }

            ent->value = v;
            return;
        }
        after = ent;
    }

    hash_entry_t *e = (hash_entry_t *) k_malloc(sizeof(hash_entry_t));
    e->value = v;
    e->key = h->keydup ? h->keydup(key) : key;
    e->next = NULL;

    after->next = e;
    ++h->item_count;
}

int hash_get(hash_t *h, uint64_t key, void **val) {
    size_t index = h->keyhsh(key) % h->bucket_count;
    if (!h->buckets[index]) {
        return -1;
    }

    for (hash_entry_t *ent = h->buckets[index]; ent; ent = ent->next) {
        if (!h->keycmp(ent->key, key)) {
            *val = ent->value;
            return 0;
        }
    }
    return -1;
}

int hash_del(hash_t *h, uint64_t key) {
    size_t index = h->keyhsh(key) % h->bucket_count;

    if (!h->buckets[index]) {
        return -1;
    }

    hash_entry_t *prev = NULL;
    for (hash_entry_t *ent = h->buckets[index]; ent; prev = ent, ent = ent->next) {
        if (!h->keycmp(ent->key, key)) {
            if (prev) {
                prev->next = ent->next;
            } else {
                h->buckets[index] = ent->next;
            }

            // k_free the entry
            if (h->keyfree) {
                h->keyfree(ent->key);
            }
            if (h->valfree) {
                h->valfree(ent->value);
            }
            k_free(ent);
            --h->item_count;
            return 0;
        }
    }

    return -1;
}
