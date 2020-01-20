#include "sys/tty.h"
#include "sys/chr.h"
#include "sys/errno.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/termios.h"
#include "sys/sched.h"
#include "sys/ring.h"
#include "sys/mm.h"
#include "sys/amd64/cpu.h"
#include "sys/amd64/hw/con.h"
#include "sys/dev.h"

#define DEV_TTY(n)          (n ## ULL)

static ssize_t tty_write(struct chrdev *tty, const void *buf, size_t pos, size_t lim);
static ssize_t tty_read(struct chrdev *tty, void *buf, size_t pos, size_t lim);
static int tty_ioctl(struct chrdev *tty, unsigned int cmd, void *arg);

struct tty_data {
    // Process group ID of the foreground group (session leader)
    pid_t fg_pgid;
    // Number
    int tty_n;
    // TODO: make console screen and keyboard character devices
};

static struct tty_data _dev_tty0_data = {
    .fg_pgid = 1,
    .tty_n = DEV_TTY(0)
};

static struct chrdev _dev_tty0 = {
    .dev_data = &_dev_tty0_data,
    .write = tty_write,
    .read = tty_read,
    .ioctl = tty_ioctl
};

static void tty_signal_group(struct chrdev *tty, int signum) {
    struct tty_data *data = tty->dev_data;
    _assert(data);
    sched_signal_group(data->fg_pgid, signum);
}

void tty_signal_write(int tty_no, int signum) {
    kdebug("Send signal %d on tty%d\n", signum, tty_no);
    switch (tty_no) {
    case 0:
        tty_signal_group(&_dev_tty0, signum);
        break;
    }
}

// This function receives keystrokes from keyboard drivers
void tty_buffer_write(int tty_no, char c) {
    switch (tty_no) {
    case 0:
        ring_putc(NULL, &_dev_tty0.buffer, c);
        break;
    }
}

void tty_init(void) {
    ring_init(&_dev_tty0.buffer, 16);

    dev_add(DEV_CLASS_CHAR, DEV_CHAR_TTY, &_dev_tty0, "tty0");
}

// TODO: multiple ttys
static ssize_t tty_write(struct chrdev *tty, const void *buf, size_t pos, size_t lim) {
    struct tty_data *data = tty->dev_data;
    _assert(data);
    if (data->tty_n != DEV_TTY(0)) {
        return -EINVAL;
    }

    // TODO: buffer sanity checks
    for (size_t i = 0; i < lim; ++i) {
        amd64_con_putc(((const char *) buf)[i]);
    }
    return lim;
}

static ssize_t tty_read(struct chrdev *tty, void *buf, size_t pos, size_t lim) {
    struct tty_data *data = tty->dev_data;
    _assert(data);
    char ibuf[16];

    if (data->tty_n != DEV_TTY(0)) {
        return -EINVAL;
    }

    if (lim == 0) {
        return 0;
    }

    size_t rem = lim;
    size_t p = 0;

    while (rem) {
        ssize_t rd = ring_read(get_cpu()->thread, &tty->buffer, ibuf, MIN(16, rem));
        if (rd < 0) {
            break;
        }
        memcpy((char *) buf + p, ibuf, rd);

        rem -= rd;
        p += rd;
    }

    return p;
}

static int tty_ioctl(struct chrdev *tty, unsigned int cmd, void *arg) {
    struct tty_data *data = tty->dev_data;
    _assert(data);

    switch (cmd) {
    case TIOCSPGRP:
        data->fg_pgid = *(pid_t *) arg;
        return 0;
    default:
        return -EINVAL;
    }
}
