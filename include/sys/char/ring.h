#pragma once
#include "sys/types.h"
#include "sys/wait.h"
#include "sys/spin.h"

struct thread;

#define RING_SIGNAL_BRK     (1 << 0)
#define RING_SIGNAL_EOF     (1 << 1)
#define RING_SIGNAL_RET     (1 << 2)
#define RING_RAW            (1 << 3)

// Ring buffer
struct ring {
    size_t rd, wr;
    size_t cap;
    char *base;
    int flags;

    // Reader notification
    struct io_notify wait;
    // Writer notification
    struct io_notify writer_wait;
};

int ring_readable(struct ring *b);
int ring_getc(struct thread *ctx, struct ring *b, char *c, int err);

void ring_signal(struct ring *b, int type);
int ring_putc(struct thread *ctx, struct ring *b, char c, int wait);
int ring_write(struct thread *ctx, struct ring *b, const void *data, size_t count, int wait);

int ring_init(struct ring *b, size_t cap);
