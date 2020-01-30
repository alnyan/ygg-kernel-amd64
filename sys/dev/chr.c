#include "sys/amd64/cpu.h"
#include "sys/dev/chr.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/sched.h"
#include "sys/errno.h"

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
