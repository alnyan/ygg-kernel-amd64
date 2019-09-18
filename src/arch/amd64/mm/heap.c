#include "arch/amd64/mm/heap.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/mm.h"

#define HEAP_MAGIC 0x1BAD83A0

typedef struct heap_block {
    uint32_t magic;
    uint32_t size;
    struct heap_block *prev, *next;
} heap_block_t;

static struct kernel_heap {
    uintptr_t phys_base;
    size_t limit;
} amd64_global_heap;
heap_t *heap_global = &amd64_global_heap;

void amd64_heap_init(heap_t *heap, uintptr_t phys_base, size_t sz) {
    heap->phys_base = phys_base;
    heap->limit = sz;

    // Create a single whole-heap block
    heap_block_t *block = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);
    block->magic = HEAP_MAGIC;
    block->next = NULL;
    block->prev = NULL;
    block->size = sz - sizeof(heap_block_t);
}

// Heap interface implementation
void *heap_alloc(heap_t *heap, size_t count) {
    heap_block_t *begin = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);

    for (heap_block_t *block = begin; block; block = block->next) {
        if (block->magic & 1) {
            continue;
        }

        if (count == block->size) {
            block->magic |= 1;
            return (void *) &block[1];
        } else if (count <= block->size + sizeof(heap_block_t)) {
            // Insert new block after this one
            heap_block_t *new_block = (heap_block_t *) (((uintptr_t) block) + sizeof(heap_block_t) + count);
            new_block->next = block->next;
            new_block->prev = block;
            new_block->size = block->size - sizeof(heap_block_t) - count;
            new_block->magic = HEAP_MAGIC;
            block->next = new_block;
            block->size = count;
            block->magic |= 1;
            return (void *) &block[1];
        }
    }

    return NULL;
}

void heap_free(heap_t *heap, void *ptr) {
    if (!ptr) {
        return;
    }

    // Check if the pointer belongs to the heap
    if (((uintptr_t) ptr) < MM_VIRTUALIZE(heap->phys_base) ||
        ((uintptr_t) ptr) > (MM_VIRTUALIZE(heap->phys_base) + heap->limit)) {
        panic("Tried to free a pointer from outside a heap\n");
    }

    // Check if ptr is in a valid block
    heap_block_t *block = (heap_block_t *) (((uintptr_t) ptr) - sizeof(heap_block_t));

    if ((block->magic & HEAP_MAGIC) != HEAP_MAGIC) {
        panic("The pointer is from an invalid block\n");
    }

    if (!(block->magic & 1)) {
        panic("Double free\n");
    }

    block->magic = HEAP_MAGIC;
    kdebug("%p is free\n", block);

    heap_block_t *prev = block->prev;
    heap_block_t *next = block->next;

    if (prev && !(prev->magic & 1)) {
        prev->next = next;
        if (next) {
            next->prev = prev;
        }
        prev->size += sizeof(heap_block_t) + block->size;
        block->magic = 0;
        block = prev;
    }

    if (next && !(next->magic & 1)) {
        block->next = next->next;
        if (next->next) {
            next->next->prev = block;
        }
        next->magic = 0;
        block->size += sizeof(heap_block_t) + next->size;
    }
}

// amd64-specific
size_t amd64_heap_blocks(const heap_t *heap) {
    size_t c = 0;
    for (const heap_block_t *block = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);
         block; block = block->next) {
        if ((block->magic & HEAP_MAGIC) != HEAP_MAGIC) {
            panic("Invalid block %u\n", c);
        }
        ++c;
    }
    return c;
}

void amd64_heap_dump(const heap_t *heap) {
    for (const heap_block_t *block = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);
         block; block = block->next) {
        if ((block->magic & HEAP_MAGIC) != HEAP_MAGIC) {
            panic("Invalid block %p\n", block);
        }

        kdebug("%p: %u %s%s\n", block, block->size, (block->magic & 1 ? "USED" : "FREE"), (block->next ? " -> " : ""));
    }
}
