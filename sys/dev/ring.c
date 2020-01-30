#include "sys/dev/ring.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"

int ring_readable(struct ring *ring) {
    // | . . . . R X X X W . . . |
    if (ring->rd <= ring->wr) {
        return ring->wr - ring->rd;
    // | X X W . . . . . . R X X |
    } else {
        return ring->wr + (ring->cap - ring->rd);
    }
}

static int ring_writable(struct ring *ring) {
    if (ring->wr == ring->cap - 1 && ring->rd == 0) {
        return 0;
    }

    // | X X X X R . . . W X X X |
    if (ring->rd <= ring->wr) {
        return ring->wr + (ring->cap - ring->rd);
    // | . . W X X X X R . . . . |
    } else {
        if (ring->rd == ring->wr + 1) {
            return 0;
        }
        return ring->rd - ring->wr;
    }
}

static void ring_advance_read(struct ring *ring) {
    if (ring->rd == ring->cap - 1) {
        ring->rd = 0;
    } else {
        ++ring->rd;
    }
}

static void ring_advance_write(struct ring *ring) {
    if (ring->wr == ring->cap - 1) {
        ring->wr = 0;
    } else {
        ++ring->wr;
    }
}

int ring_getc(struct thread *ctx, struct ring *ring, char *c, int err) {
    if (err) {
        if (!ring_readable(ring)) {
            return -1;
        }
    } else {
        do {
            if (ring->flags & (RING_SIGNAL_BRK | RING_SIGNAL_EOF)) {
                ring->flags &= ~RING_SIGNAL_BRK;
                // TODO: send SIGINT on break
                return -1;
            }

            if (!ring_readable(ring)) {
                panic("Feck\n");
                //asm volatile ("sti; hlt; cli");
            } else {
                break;
            }
        } while (1);
    }

    *c = ring->base[ring->rd];
    ring_advance_read(ring);
    return 0;
}

int ring_putc(struct thread *ctx, struct ring *ring, char c, int wait) {
    if (wait) {
        while (!ring_writable(ring)) {
            asm volatile ("sti; hlt; cli");
        }
    }

    ring->base[ring->wr] = c;
    ring_advance_write(ring);
    return 0;
}

int ring_write(struct thread *ctx, struct ring *ring, const void *buf, size_t len, int wait) {
    for (size_t i = 0; i < len; ++i) {
        if (ring_putc(ctx, ring, ((const char *) buf)[i], wait) != 0) {
            return -1;
        }
    }
    return 0;
}

void ring_signal(struct ring *r, int s) {
    r->flags |= s;
}

int ring_init(struct ring *r, size_t cap) {
    r->cap = cap;
    r->rd = 0;
    r->wr = 0;
    r->flags = 0;
    if (!(r->base = kmalloc(cap))) {
        return -1;
    }
    return 0;
}

