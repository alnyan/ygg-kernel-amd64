// TODO: SMP/thread safety
#include "sys/block/cache.h"
#include "sys/block/blk.h"
#include "sys/mem/phys.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/mm.h"

#define LRU_PAGE_DIRTY          1
#define LRU_PAGE_MASK           (~LRU_PAGE_DIRTY)

struct lru_node {
    uintptr_t block_address;
    union {
        uintptr_t page;             // For LRU
        struct lru_node *index;     // For index hash
    };
    struct lru_node *prev, *next;
};

////

static inline size_t lru_hash_reference(uintptr_t address) {
    return address;
}

static struct lru_node *lru_hash_lookup(struct lru_hash *hash, uintptr_t address) {
    size_t bucket_index = lru_hash_reference(address) % hash->bucket_count;

    for (struct lru_node *node = hash->bucket_heads[bucket_index]; node; node = node->next) {
        if (node->block_address == address) {
            return node;
        }
    }

    return NULL;
}

static int lru_hash_insert(struct lru_hash *hash, uintptr_t address, struct lru_node *index) {
    size_t bucket_index = lru_hash_reference(address) % hash->bucket_count;
    struct lru_node *node = kmalloc(sizeof(struct lru_node));
    _assert(node);

    node->block_address = address;
    node->index = index;

    node->prev = hash->bucket_tails[bucket_index];
    if (node->prev) {
        hash->bucket_tails[bucket_index]->next = node;
    } else {
        hash->bucket_heads[bucket_index] = node;
    }
    hash->bucket_tails[bucket_index] = node;
    node->next = NULL;

    return 0;
}

static int lru_hash_remove(struct lru_hash *hash, uintptr_t address) {
    size_t bucket_index = lru_hash_reference(address) % hash->bucket_count;
    struct lru_node *node = lru_hash_lookup(hash, address);
    if (!node) {
        return -1;
    }

    struct lru_node *prev = node->prev;
    struct lru_node *next = node->next;

    if (prev) {
        prev->next = next;
    } else {
        hash->bucket_heads[bucket_index] = next;
    }
    if (next) {
        next->prev = prev;
    } else {
        hash->bucket_tails[bucket_index] = prev;
    }

    kfree(node);

    return 0;
}

static void lru_hash_init(struct lru_hash *hash, size_t capacity) {
    hash->bucket_count = capacity;
    hash->bucket_heads = kmalloc(sizeof(struct lru_node *) * capacity);
    hash->bucket_tails = kmalloc(sizeof(struct lru_node *) * capacity);
    _assert(hash->bucket_tails && hash->bucket_heads);
    memset(hash->bucket_heads, 0, sizeof(struct lru_node *) * capacity);
    memset(hash->bucket_tails, 0, sizeof(struct lru_node *) * capacity);
}

////

void block_cache_init(struct block_cache *cache, struct blkdev *blk, size_t page_size, size_t capacity) {
    cache->page_size = page_size;
    cache->capacity = capacity;
    cache->queue_head = NULL;
    cache->queue_tail = NULL;
    cache->size = 0;
    cache->blk = blk;
    lru_hash_init(&cache->index_hash, 32);
}

void block_cache_release(struct block_cache *cache) {
    _assert(!cache->queue_head);
    _assert(!cache->queue_tail);
    kfree(cache->index_hash.bucket_heads);
    kfree(cache->index_hash.bucket_tails);
    memset(cache, 0, sizeof(struct block_cache));
}

static void block_cache_queue_push_node(struct block_cache *cache, struct lru_node *node) {
    node->next = cache->queue_head;
    if (cache->queue_head) {
        cache->queue_head->prev = node;
    } else {
        cache->queue_tail = node;
    }
    cache->queue_head = node;
    node->prev = NULL;
    ++cache->size;
}

static void block_cache_queue_pop_tail(struct block_cache *cache) {
    struct lru_node *last = cache->queue_tail;
    _assert(last);
    _assert(lru_hash_remove(&cache->index_hash, last->block_address) == 0);
    cache->queue_tail = last->prev;
    if (cache->queue_tail) {
        cache->queue_tail->next = NULL;
    } else {
        _assert(cache->size == 1);
        cache->queue_head = NULL;
    }
    --cache->size;
}

static struct lru_node *block_cache_queue_push_head(struct block_cache *cache, uintptr_t address, uintptr_t page) {
    struct lru_node *node = kmalloc(sizeof(struct lru_node));
    node->block_address = address;
    node->page = page;
    _assert(node);
    block_cache_queue_push_node(cache, node);
    return node;
}

static void block_cache_queue_erase(struct block_cache *cache, struct lru_node *node) {
    struct lru_node *prev = node->prev;
    struct lru_node *next = node->next;

    if (prev) {
        prev->next = next;
    } else {
        cache->queue_head = next;
    }
    if (next) {
        next->prev = prev;
    } else {
        cache->queue_tail = prev;
    }

    --cache->size;
}

static void block_cache_page_release(struct block_cache *cache, uintptr_t address, uintptr_t page) {
    if (page & LRU_PAGE_DIRTY) {
        kdebug("Block cache: write page %p\n", page & LRU_PAGE_MASK);
        _assert(blk_page_sync(cache->blk, address * cache->page_size, page & LRU_PAGE_MASK) == 0);
    }
    mm_phys_free_page(page & LRU_PAGE_MASK);
}

static uintptr_t block_cache_page_alloc(struct block_cache *cache) {
    // Other sizes are not supported
    _assert(cache->page_size == MM_PAGE_SIZE);
    return mm_phys_alloc_page(PU_CACHE);
}

void block_cache_mark_dirty(struct block_cache *cache, uintptr_t address) {
    // Convert address to block index
    _assert((address % cache->page_size) == 0);
    address /= cache->page_size;

    // Lookup index in cache
    struct lru_node *index = lru_hash_lookup(&cache->index_hash, address);
    // Must be present in cache
    _assert(index);
    struct lru_node *node = index->index;
    _assert(node);

    node->page |= LRU_PAGE_DIRTY;
}

int block_cache_get(struct block_cache *cache, uintptr_t address, uintptr_t *page) {
    // Convert address to block index
    _assert((address % cache->page_size) == 0);
    address /= cache->page_size;

    // Lookup index in cache
    struct lru_node *index = lru_hash_lookup(&cache->index_hash, address);

    if (!index) {
        // Remove the least recently used element
        if (cache->capacity == cache->size) {
            _assert(cache->queue_tail);
            uintptr_t old_page = cache->queue_tail->page;
            block_cache_page_release(cache, cache->queue_tail->block_address, cache->queue_tail->page);
            block_cache_queue_pop_tail(cache);
        }

        uintptr_t result = block_cache_page_alloc(cache);
        _assert(result != MM_NADDR);

        // Insert a new reference
        struct lru_node *node = block_cache_queue_push_head(cache, address, result);
        _assert(node);
        lru_hash_insert(&cache->index_hash, address, node);

        *page = result;
        return -1;
    } else {
        // Put reference to the head
        struct lru_node *node = index->index;
        _assert(node);
        _assert(node->block_address == index->block_address);
        *page = node->page & LRU_PAGE_MASK;

        block_cache_queue_erase(cache, node);
        block_cache_queue_push_node(cache, node);

        return 0;
    }
}

void block_cache_flush(struct block_cache *cache) {
    // Write all "dirty" pages and release the whole cache to force
    // reload from disk
    struct lru_node *node;

    while ((node = cache->queue_tail) != NULL) {
        block_cache_page_release(cache, node->block_address, node->page);
        block_cache_queue_pop_tail(cache);
    }
}
