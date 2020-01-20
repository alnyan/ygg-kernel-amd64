#include "sys/amd64/cpu.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/sched.h"
#include "sys/errno.h"
#include "sys/chr.h"

int chr_ioctl(struct chrdev *chr, unsigned int cmd, void *arg) {
    if (!chr) {
        return -ENODEV;
    }
    if (!chr->ioctl) {
        return -EINVAL;
    }
    return chr->ioctl(chr, cmd, arg);
}

ssize_t chr_write(struct chrdev *chr, const void *buf, size_t off, size_t count) {
    if (!chr) {
        return -ENODEV;
    }
    if (!chr->write) {
        return -EROFS;
    }
    return chr->write(chr, buf, off, count);
}

ssize_t chr_read(struct chrdev *chr, void *buf, size_t off, size_t count) {
    if (!chr) {
        return -ENODEV;
    }
    if (!chr->read) {
        return -EINVAL;
    }
    return chr->read(chr, buf, off, count);
}

ssize_t chr_read_ring(struct chrdev *chr, void *buf, size_t off, size_t lim) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    char ibuf[16];

    if (lim == 0) {
        return 0;
    }

    size_t rem = lim;
    size_t p = 0;

    while (rem) {
        if (chr->buffer.post) {
            break;
        }
        ssize_t rd = ring_read(thr, &chr->buffer, ibuf, MIN(16, rem));
        if (rd <= 0) {
            // Interrupt or end of stream
            break;
        }
        memcpy((char *) buf + p, ibuf, rd);

        rem -= rd;
        p += rd;

        if (thr->flags & THREAD_INTERRUPTED) {
            thr->flags &= ~THREAD_INTERRUPTED;
            return p;
        }
    }
    // Reset post bit
    chr->buffer.post = 0;

    return p;
}
