#pragma once
#include "sys/types.h"

struct chrdev {
    void *dev_data;

    ssize_t (*write) (struct chrdev *chr, const void *buf, size_t pos, size_t lim);
    ssize_t (*read) (struct chrdev *chr, void *buf, size_t pos, size_t lim);
};

ssize_t chr_write(struct chrdev *chr, const void *buf, size_t pos, size_t lim);
ssize_t chr_read(struct chrdev *chr, void *buf, size_t pos, size_t lim);
