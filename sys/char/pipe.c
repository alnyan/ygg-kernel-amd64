#include "arch/amd64/cpu.h"
#include "sys/char/pipe.h"
#include "sys/char/ring.h"
#include "user/errno.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"

#define PIPE_RING_SIZE      1024

static ssize_t pipe_vnode_write(struct ofile *of, const void *buf, size_t count);
static ssize_t pipe_vnode_read(struct ofile *of, void *buf, size_t count);
static void pipe_vnode_close(struct ofile *of);

static struct vnode_operations pipe_vnode_ops = {
    .close = pipe_vnode_close,
    .write = pipe_vnode_write,
    .read = pipe_vnode_read
};

int pipe_create(struct ofile **_read, struct ofile **_write) {
    // TODO: ofile_create
    struct ofile *read_end = kmalloc(sizeof(struct ofile));
    _assert(read_end);
    struct ofile *write_end = kmalloc(sizeof(struct ofile));
    _assert(write_end);

    struct ring *pipe_ring = kmalloc(sizeof(struct ring));
    _assert(pipe_ring);
    ring_init(pipe_ring, PIPE_RING_SIZE);

    struct vnode *vnode = vnode_create(VN_REG, NULL);
    _assert(vnode);
    vnode->op = &pipe_vnode_ops;
    vnode->flags |= VN_MEMORY;

    read_end->flags = OF_READABLE;
    read_end->file.vnode = vnode;
    read_end->file.refcount = 1;
    read_end->file.priv_data = pipe_ring;

    write_end->flags = OF_WRITABLE;
    write_end->file.vnode = vnode;
    write_end->file.refcount = 1;
    write_end->file.priv_data = pipe_ring;

    *_read = read_end;
    *_write = write_end;

    return 0;
}

static ssize_t pipe_vnode_write(struct ofile *of, const void *buf, size_t count) {
    struct thread *thr = thread_self;
    _assert(thr);
    struct ring *r = of->file.priv_data;
    _assert(r);

    ssize_t res = ring_write(thr, r, buf, count, 0);
    if (res > 0) {
        ring_signal(r, RING_SIGNAL_RET);
    }
    return res;
}

static ssize_t pipe_vnode_read(struct ofile *of, void *buf, size_t count) {
    struct thread *thr = thread_self;
    _assert(thr);
    struct ring *r = of->file.priv_data;
    _assert(r);

    size_t rd = 0;
    char c;
    size_t rem = count;
    char *wr = buf;

    while (rem) {
        if (ring_getc(thr, r, &c, 0) < 0) {
            break;
        }

        *wr++ = c;
        ++rd;
        --rem;

        if (!ring_readable(r) && (r->flags & RING_SIGNAL_RET)) {
            r->flags &= ~RING_SIGNAL_RET;
            return rd;
        }
    }

    if (rd == 0) {
        return -EPIPE;
    }

    return rd;
}

static void pipe_vnode_close(struct ofile *of) {
    _assert(of && of->file.priv_data);
    if (of->flags & OF_WRITABLE) {
        ring_signal(of->file.priv_data, RING_SIGNAL_EOF);
    }
    //kfree(((struct ring *) of->file.priv_data)->base);
    //kfree(of->file.priv_data);
}
