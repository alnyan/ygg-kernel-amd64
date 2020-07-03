#include "sys/char/ring.h"
#include "sys/thread.h"
#include "user/errno.h"
#include "sys/assert.h"
#include "sys/sched.h"
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

//static void ring_notify_reader(struct ring *ring) {
//    if (ring->reader_head) {
//        struct thread *thr = ring->reader_head;
//        ring->reader_head = NULL; //thr->wait_next;
//
//        sched_queue(thr);
//    }
//}

int ring_getc(struct thread *ctx, struct ring *ring, char *c, int err) {
    int res;
    if (err) {
        if (!ring_readable(ring)) {
            return -1;
        }
    } else {
        do {
            // TODO: better handling of EOF condition?
            if (ring->flags & (RING_SIGNAL_BRK | RING_SIGNAL_EOF)) {
                if (!ring_readable(ring)) {
                    ring->flags &= ~RING_SIGNAL_BRK;
                    return -1;
                }
            }

            if (!ring_readable(ring)) {
                if ((res = thread_wait_io(ctx, &ring->wait)) != 0) {
                    _assert(res == -EINTR);
                    return res;
                }
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
            panic("Not implemented\n");
        }
    }

    ring->base[ring->wr] = c;
    ring_advance_write(ring);
    thread_notify_io(&ring->wait);

    return 0;
}

int ring_write(struct thread *ctx, struct ring *ring, const void *buf, size_t len, int wait) {
    for (size_t i = 0; i < len; ++i) {
        if (ring_putc(ctx, ring, ((const char *) buf)[i], wait) != 0) {
            return -1;
        }
    }
    return len;
}

void ring_signal(struct ring *r, int s) {
    r->flags |= s;
    thread_notify_io(&r->wait);
}

int ring_init(struct ring *r, size_t cap) {
    r->cap = cap;
    r->rd = 0;
    r->wr = 0;
    r->flags = 0;
    thread_wait_io_init(&r->wait);

    if (!(r->base = kmalloc(cap))) {
        return -1;
    }
    return 0;
}

