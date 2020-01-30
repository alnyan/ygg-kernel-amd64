#pragma once
#include <sys/types.h>
#include <stdint.h>

typedef struct hash_entry {
    uint64_t key;
    void *value;
    struct hash_entry *next;
} hash_entry_t;

typedef struct {
    int (*keycmp) (uint64_t, uint64_t);
    uint64_t (*keyhsh) (uint64_t);
    uint64_t (*keydup) (uint64_t);
    void (*keyfree) (uint64_t);
    void (*valfree) (void *);

    size_t bucket_count;
    size_t item_count;
    hash_entry_t **buckets;
} hash_t;


void hash_init(hash_t *h, int nb);
void hash_walk(hash_t *h, int (*wlkfn) (hash_entry_t *));
void hash_clear(hash_t *h);

void hash_put(hash_t *h, uint64_t key, void *value);
int hash_del(hash_t *h, uint64_t key);
int hash_get(hash_t *h, uint64_t key, void **value);

// Helper functions
int hash_u64_keycmp(uint64_t v0, uint64_t v1);
uint64_t hash_u64_keyhsh(uint64_t v0);
