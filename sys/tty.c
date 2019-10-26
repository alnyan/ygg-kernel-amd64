#include "sys/tty.h"
#include "sys/chr.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/string.h"
#include "sys/ring.h"
#include "sys/mm.h"
#include "sys/amd64/cpu.h"

#define DEV_TTY(n)          (n ## ULL)
#define DEV_DATA_TTY(n)     ((void *) n ## ULL)
#define DEV_DATA_GETTTY(d)  ((uint64_t) (d)->dev_data)

static ssize_t tty_write(struct chrdev *tty, const void *buf, size_t pos, size_t lim);
static ssize_t tty_read(struct chrdev *tty, void *buf, size_t pos, size_t lim);

static struct chrdev _dev_tty0 = {
    .name = "tty0",
    .dev_data = DEV_DATA_TTY(0),
    .write = tty_write,
    .read = tty_read
};

static vnode_t _tty0 = {
    .type = VN_CHR,
    .dev = &_dev_tty0
};
vnode_t *tty0 = &_tty0;

static struct ring tty_ring = {0};

// This function receives keystrokes from keyboard drivers
void tty_buffer_write(int tty_no, char c) {
    ring_putc(&tty_ring, c);
}

void tty_init(void) {
    ring_init(&tty_ring, 16);
}

// TODO: multiple ttys
static ssize_t tty_write(struct chrdev *tty, const void *buf, size_t pos, size_t lim) {
    uint64_t tty_no = DEV_DATA_GETTTY(tty);

    if (tty_no != DEV_TTY(0)) {
        return -EINVAL;
    }

    // TODO: buffer sanity checks
    for (size_t i = 0; i < lim; ++i) {
        debugc(DEBUG_INFO, ((const char *) buf)[i]);
    }
    return lim;
}

static ssize_t tty_read(struct chrdev *tty, void *buf, size_t pos, size_t lim) {
    uint64_t tty_no = DEV_DATA_GETTTY(tty);
    char ibuf[16];

    if (tty_no != DEV_TTY(0)) {
        return -EINVAL;
    }

    if (lim == 0) {
        return 0;
    }

    size_t rem = lim;
    size_t p = 0;

    while (rem) {
        ssize_t rd = ring_read(get_cpu()->thread, &tty_ring, ibuf, MIN(16, rem));
        if (rd < 0) {
            break;
        }
        memcpy((char *) buf + p, ibuf, rd);

        rem -= rd;
    }

    return p;
}
