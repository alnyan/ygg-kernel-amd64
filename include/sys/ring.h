#pragma once
#include "sys/types.h"
#include "sys/spin.h"

// Ring buffer
struct ring {
    char *data;
    size_t wrptr;
    size_t rdptr;
    size_t cap;
    spin_t lock;
    int post;
    struct thread *listener;
};

void ring_post(struct ring *b);
size_t ring_avail(struct ring *b);
ssize_t ring_read(struct thread *ctx, struct ring *b, void *buf, size_t lim);
int ring_putc(struct thread *ctx, struct ring *b, char c);
void ring_init(struct ring *b, size_t cap);
