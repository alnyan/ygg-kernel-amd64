#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/ring.h"
#include "sys/heap.h"

size_t ring_avail(struct ring *ring) {
    if (ring->rdptr == ring->wrptr) {
        return 0;
    }

    if (ring->rdptr > ring->wrptr) {
        return (ring->cap - ring->rdptr) + ring->wrptr;
    } else {
        return (ring->wrptr - ring->rdptr);
    }
}

size_t ring_writable(struct ring *ring) {
    if (ring->rdptr == ring->wrptr) {
        return ring->cap - 1;
    }

    if (ring->rdptr > ring->wrptr) {
        return ring->rdptr - ring->wrptr - 1;
    } else {
        return (ring->cap - ring->wrptr) + ring->rdptr - 1;
    }
}

void ring_post(struct ring *ring) {
    ring->post = 1;
}

static inline void ring_advance_read(struct ring *ring) {
	ring->rdptr++;
    if (ring->rdptr == ring->cap) {
        ring->rdptr = 0;
    }
}

static inline void ring_advance_write(struct ring *ring) {
	ring->wrptr++;
	if (ring->wrptr == ring->cap) {
	    ring->wrptr = 0;
	}
}

int ring_getc(struct thread *ctx, struct ring *ring, char *out) {
    uintptr_t flags;

    //spin_lock_irqsave(&ring->lock, &flags);

    do {
        if (ring_avail(ring)) {
            break;
        }
        //spin_release(&ring->lock);
        ctx->flags |= THREAD_WAITING;
        ctx->flags &= ~THREAD_INTERRUPTED;
        asm volatile ("sti; hlt");
        if (ring->post) {
            break;
        }
        if (ctx->flags & (THREAD_INTERRUPTED | THREAD_EOF)) {
            return -1;
        }
        ctx->flags &= ~THREAD_WAITING;
        //spin_lock(&ring->lock);
    } while (1);


    *out = ring->data[ring->rdptr];
    ring_advance_read(ring);

    //spin_release_irqrestore(&ring->lock, &flags);

    return 0;
}

ssize_t ring_read(struct thread *ctx, struct ring *ring, void *buf, size_t count) {
    size_t rem = count;
    size_t pos = 0;

    while (rem) {
        if (ring->post) {
            break;
        }

        if (ring_getc(ctx, ring, (char *) buf + pos) != 0) {
            break;
        }

        --rem;
        ++pos;
    }

    if (pos == 0) {
        return -1;
    }

	return pos;
}

int ring_putc(struct thread *ctx, struct ring *ring, char c) {
    uintptr_t flags;

    //spin_lock_irqsave(&ring->lock, &flags);

    do {
        if (ring_writable(ring)) {
            break;
        }
        //spin_release(&ring->lock);
        if (ctx) {
            asm volatile ("sti; hlt");
        } else {
            asm volatile ("sti; hlt");
        }
        //spin_lock(&ring->lock);
    } while (1);


    ring->data[ring->wrptr] = c;
    ring_advance_write(ring);

    //spin_release_irqrestore(&ring->lock, &flags);

	return 0;
}

void ring_init(struct ring *ring, size_t cap) {
    ring->lock = 0;
    ring->wrptr = 0;
    ring->rdptr = 0;
    ring->data = kmalloc(cap);
    _assert(ring->data);
    ring->cap = cap;
}
