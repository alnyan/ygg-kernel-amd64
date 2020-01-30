#pragma once
#include "sys/dev/ring.h"
#include "sys/termios.h"
#include "sys/types.h"

struct chrdev {
    void *dev_data;

    struct ring buffer;
    enum {
        CHRDEV_GENERIC = 0,
        CHRDEV_TTY
    } type;

    // TODO: move tty info somewhere else?
    struct termios tc;

    int (*ioctl) (struct chrdev *chr, unsigned int cmd, void *arg);
    ssize_t (*write) (struct chrdev *chr, const void *buf, size_t pos, size_t lim);
    ssize_t (*read) (struct chrdev *chr, void *buf, size_t pos, size_t lim);
};

int chr_ioctl(struct chrdev *chr, unsigned int cmd, void *arg);
ssize_t chr_write(struct chrdev *chr, const void *buf, size_t pos, size_t lim);
ssize_t chr_read(struct chrdev *chr, void *buf, size_t pos, size_t lim);
