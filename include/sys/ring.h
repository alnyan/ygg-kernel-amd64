#pragma once
#include "sys/types.h"
#include "sys/spin.h"

struct thread;

#define RING_SIGNAL_BRK     (1 << 0)
#define RING_SIGNAL_EOF     (1 << 1)
#define RING_SIGNAL_RET     (1 << 2)

// Ring buffer
struct ring {
    size_t rd, wr;
    size_t cap;
    char *base;
    int flags;
};

int ring_readable(struct ring *b);
int ring_getc(struct thread *ctx, struct ring *b, char *c, int err);

void ring_signal(struct ring *b, int type);
int ring_putc(struct thread *ctx, struct ring *b, char c, int wait);
int ring_write(struct thread *ctx, struct ring *b, const void *data, size_t count, int wait);

int ring_init(struct ring *b, size_t cap);
