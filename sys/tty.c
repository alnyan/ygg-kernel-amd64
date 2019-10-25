#include "sys/tty.h"
#include "sys/chr.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/string.h"
#include "sys/mm.h"

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

// TODO: this seems very ugly
static char *_tty0_reading_now = NULL;
static size_t _tty0_left = 0;

// This function receives keystrokes from keyboard drivers
void tty_buffer_write(int tty_no, char c) {
    asm volatile ("cli");
    if (tty_no != 0) {
        panic("Not implemented\n");
    }

    if (!_tty0_left) {
        // Ignore keystrokes sent while we're not reading
        // This is bad and should be reimplemented by
        // storing key data inside a ring buffer
        return;
    }

    // No safety
    *_tty0_reading_now++ = c;
    --_tty0_left;
}

void tty_init(void) {
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

#define MIN(x, y) ((x) < (y) ? (x) : (y))
    size_t read = 0;

    while (lim > 0) {
        // XXX: This is very ugly
        //      Better just send the task to some kind of "busy"
        //      state and then wait
        size_t read_now = MIN(16, lim);
        _tty0_left = read_now;
        _tty0_reading_now = ibuf;
        while (1) {
            asm volatile ("cli");
            if (_tty0_left == 0) {
                break;
            }
            asm volatile ("sti; hlt");
        }
        // Read 16 byte block
        lim -= read_now;
        // TODO: memcpy_kernel_to_user
        memcpy((void *) ((uintptr_t) buf + read), ibuf, read_now);
        read += read_now;
    }


    return 16;
}
