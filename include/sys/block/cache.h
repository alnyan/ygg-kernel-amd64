#pragma once
#include "sys/types.h"

struct lru_node;

struct lru_hash {
    size_t bucket_count;
    struct lru_node **bucket_heads;
    struct lru_node **bucket_tails;
};

struct block_cache {
    size_t page_size;
    size_t capacity, size;
    struct blkdev *blk;
    struct lru_node *queue_head, *queue_tail;
    struct lru_hash index_hash;
    // For global cache tracking
    struct block_cache *g_prev, *g_next;
};

void block_cache_init(struct block_cache *cache, struct blkdev *blk, size_t page_size, size_t page_capacity);
void block_cache_flush(struct block_cache *cache);
int block_cache_get(struct block_cache *cache, uintptr_t address, uintptr_t *page);
void block_cache_mark_dirty(struct block_cache *cache, uintptr_t address);
