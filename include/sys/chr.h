#pragma once
#include "sys/types.h"
#include "sys/ring.h"

struct chrdev {
    void *dev_data;
    struct ring buffer;

    int (*ioctl) (struct chrdev *chr, unsigned int cmd, void *arg);
    ssize_t (*write) (struct chrdev *chr, const void *buf, size_t pos, size_t lim);
    ssize_t (*read) (struct chrdev *chr, void *buf, size_t pos, size_t lim);
};

int chr_ioctl(struct chrdev *chr, unsigned int cmd, void *arg);
ssize_t chr_write(struct chrdev *chr, const void *buf, size_t pos, size_t lim);
ssize_t chr_read(struct chrdev *chr, void *buf, size_t pos, size_t lim);
