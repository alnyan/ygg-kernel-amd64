#include "sys/char/pipe.h"
#include "sys/char/ring.h"
#include "sys/thread.h"
#include "user/errno.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "fs/ofile.h"
#include "sys/heap.h"

#define PIPE_RING_SIZE      1024

static int pipe_vnode_open(struct ofile *of, int opt);
static ssize_t pipe_vnode_write(struct ofile *of, const void *buf, size_t count);
static ssize_t pipe_vnode_read(struct ofile *of, void *buf, size_t count);
static int pipe_vnode_stat(struct vnode *vn, struct stat *st);
static void pipe_vnode_close(struct ofile *of);

static struct vnode_operations pipe_vnode_ops = {
    .open = pipe_vnode_open,
    .close = pipe_vnode_close,
    .write = pipe_vnode_write,
    .read = pipe_vnode_read,
    .stat = pipe_vnode_stat
};

int pipe_create(struct ofile **_read, struct ofile **_write) {
    struct ofile *read_end = ofile_create();
    struct ofile *write_end = ofile_create();

    struct ring *pipe_ring = kmalloc(sizeof(struct ring));
    _assert(pipe_ring);
    ring_init(pipe_ring, PIPE_RING_SIZE);

    struct vnode *vnode = vnode_create(VN_REG, NULL);
    _assert(vnode);
    vnode->op = &pipe_vnode_ops;
    vnode->flags |= VN_MEMORY;

    read_end->flags = OF_READABLE;
    read_end->file.vnode = vnode;
    read_end->file.priv_data = pipe_ring;

    write_end->flags = OF_WRITABLE;
    write_end->file.vnode = vnode;
    write_end->file.priv_data = pipe_ring;

    *_read = read_end;
    *_write = write_end;

    return 0;
}

int pipe_fifo_create(struct vnode *nod) {
    _assert(nod);
    nod->op = &pipe_vnode_ops;
    nod->flags |= VN_MEMORY;

    struct ring *pipe_ring = kmalloc(sizeof(struct ring));
    _assert(pipe_ring);
    ring_init(pipe_ring, PIPE_RING_SIZE);

    nod->fs_data = pipe_ring;

    return 0;
}

static ssize_t pipe_vnode_write(struct ofile *of, const void *buf, size_t count) {
    struct thread *thr = thread_self;
    _assert(thr);
    struct ring *r = of->file.priv_data;
    _assert(r);

    ssize_t res = ring_write(thr, r, buf, count, 1);
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

    return rd;
}

static void pipe_vnode_close(struct ofile *of) {
    _assert(of && of->file.priv_data);
    ring_signal(of->file.priv_data, RING_SIGNAL_EOF);
    // TODO: this
    //kfree(((struct ring *) of->file.priv_data)->base);
    //kfree(of->file.priv_data);
}

static int pipe_vnode_open(struct ofile *of, int opt) {
    _assert(of->file.vnode);
    // TODO: allow only one reader and many writers
    of->file.pos = 0;
    of->file.priv_data = of->file.vnode->fs_data;
    return 0;
}

static int pipe_vnode_stat(struct vnode *vn, struct stat *st) {
    _assert(vn && st);
    struct ring *pipe_ring = vn->fs_data;
    _assert(pipe_ring);

    st->st_size = pipe_ring->cap;
    st->st_blksize = pipe_ring->cap;
    st->st_blocks = 1;

    st->st_ino = 0;

    st->st_uid = vn->uid;
    st->st_gid = vn->gid;
    st->st_mode = vn->mode | S_IFIFO;

    st->st_atime = system_boot_time;
    st->st_mtime = system_boot_time;
    st->st_ctime = system_boot_time;

    st->st_nlink = 1;
    st->st_rdev = 0;
    st->st_dev = 0;

    return 0;
}
