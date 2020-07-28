#include "arch/amd64/mm/heap.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/spin.h"
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

static spin_t heap_lock = 0;

#if defined(HEAP_TRACE)
void heap_trace(int type, const char *file, const char *func, int line, void *ptr, size_t count) {
    if (type == HEAP_TRACE_ALLOC) {
        // Don't want fucktons of ACPI crap
        if (strncmp(func, "Acpi", 4)) {
            debugf(DEBUG_DEFAULT, "\033[31m%s:%u: %s allocated %S == %p\033[0m\n", file, line, func, count, ptr);
        }
    } else {
        // Don't want fucktons of ACPI crap
        if (strncmp(func, "Acpi", 4)) {
            debugf(DEBUG_DEFAULT, "\033[32m%s:%u: %s freed %p\033[0m\n", file, line, func, ptr);
        }
    }
}
#endif

void heap_stat(heap_t *heap, struct heap_stat *st) {
    uintptr_t irq;
    spin_lock_irqsave(&heap_lock, &irq);
    heap_block_t *begin = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);

    st->alloc_count = 0;
    st->alloc_size = 0;
    st->free_size = 0;
    st->total_size = heap->limit;

    for (heap_block_t *block = begin; block; block = block->next) {
        if ((block->magic & HEAP_MAGIC) != HEAP_MAGIC) {
            panic("Broken heap");
        }
        if (block->magic & 1) {
            ++st->alloc_count;
            st->alloc_size += block->size;
        } else {
            st->free_size += block->size;
        }
    }
    spin_release_irqrestore(&heap_lock, &irq);
}

void amd64_heap_init(heap_t *heap, uintptr_t phys_base, size_t sz) {
    heap->phys_base = phys_base;
    heap->limit = sz;

    // Create a single whole-heap block
    heap_block_t *block = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);

    uint32_t s = 13;
    for (size_t i = 0; i < sz; ++i) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        ((uint8_t *) block)[i] = s;
    }

    block->magic = HEAP_MAGIC;
    block->next = NULL;
    block->prev = NULL;
    block->size = sz - sizeof(heap_block_t);
}

// Heap interface implementation
void *heap_alloc(heap_t *heap, size_t count) {
    uintptr_t irq;
    spin_lock_irqsave(&heap_lock, &irq);
    heap_block_t *begin = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);

    // Some alignment fuck ups led me to this
    count = (count + 15) & ~15;

    for (heap_block_t *block = begin; block; block = block->next) {
        if ((block->magic & HEAP_MAGIC) != HEAP_MAGIC) {
            panic("Heap is broken: magic %08x, %p (%lu), size could be %S\n", block->magic, block, (uintptr_t) block - (uintptr_t) begin, block->size);
        }
    }

    for (heap_block_t *block = begin; block; block = block->next) {
        if (block->magic & 1) {
            continue;
        }

        if (count == block->size) {
            block->magic |= 1;
            spin_release_irqrestore(&heap_lock, &irq);
            return (void *) ((uintptr_t) block + sizeof(heap_block_t));
        } else if (block->size >= count + sizeof(heap_block_t)) {
            // Insert new block after this one
            heap_block_t *cur_next = block->next;
            heap_block_t *new_block = (heap_block_t *) (((uintptr_t) block) + sizeof(heap_block_t) + count);
            if (cur_next) {
                cur_next->prev = new_block;
            }
            new_block->next = cur_next;
            new_block->prev = block;
            new_block->size = block->size - sizeof(heap_block_t) - count;
            new_block->magic = HEAP_MAGIC;
            block->next = new_block;
            block->size = count;
            block->magic |= 1;
            spin_release_irqrestore(&heap_lock, &irq);
            return (void *) ((uintptr_t) block + sizeof(heap_block_t));
        }
    }

    spin_release_irqrestore(&heap_lock, &irq);
    return NULL;
}

void heap_free(heap_t *heap, void *ptr) {
    if (!ptr) {
        return;
    }
    uintptr_t irq;
    spin_lock_irqsave(&heap_lock, &irq);

    if (ptr == (void *) 0xffffff00042ddd70) {
    }

    // Check if the pointer belongs to the heap
    if (((uintptr_t) ptr) < MM_VIRTUALIZE(heap->phys_base) ||
        ((uintptr_t) ptr) > (MM_VIRTUALIZE(heap->phys_base) + heap->limit)) {
        panic("Tried to free a pointer from outside a heap: %p\n", ptr);
    }

    // Check if ptr is in a valid block
    heap_block_t *block = (heap_block_t *) (((uintptr_t) ptr) - sizeof(heap_block_t));

    assert((block->magic & HEAP_MAGIC) == HEAP_MAGIC, "Corrupted heap block magic: %p\n", ptr);
    assert(block->magic & 1, "Double free error (kheap): %p\n", ptr);

    block->magic = HEAP_MAGIC;
    //kdebug("%p is free\n", block);
    heap_block_t *begin = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);

    for (heap_block_t *block = begin; block; block = block->next) {
        if ((block->magic & HEAP_MAGIC) != HEAP_MAGIC) {
            panic("Heap is broken: magic %08x, %p (%lu), size could be %S\n", block->magic, block, (uintptr_t) block - (uintptr_t) begin, block->size);
        }
    }

    heap_block_t *prev = block->prev;
    heap_block_t *next = block->next;

    // TODO: fix this
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

    spin_release_irqrestore(&heap_lock, &irq);
}

// amd64-specific
size_t amd64_heap_blocks(const heap_t *heap) {
    size_t c = 0;
    for (const heap_block_t *block = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);
         block; block = block->next) {
        assert((block->magic & HEAP_MAGIC) == HEAP_MAGIC, "Corrupted heap block magic\n");
        ++c;
    }
    return c;
}

void amd64_heap_dump(const heap_t *heap) {
    for (const heap_block_t *block = (heap_block_t *) MM_VIRTUALIZE(heap->phys_base);
         block; block = block->next) {
        assert((block->magic & HEAP_MAGIC) == HEAP_MAGIC, "Corrupted heap block magic\n");

        kdebug("%p: %S %s%s\n", block, block->size, (block->magic & 1 ? "USED" : "FREE"), (block->next ? " -> " : ""));
    }
}
