#include "sys/fs/pty.h"
#include "sys/fs/ofile.h"
#include "sys/fs/node.h"
#include "sys/amd64/cpu.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/errno.h"
#include "sys/heap.h"

#define PTY_RING_SIZE       256

static ssize_t pty_master_write(struct chrdev *dev, const void *buf, size_t count, size_t off);
static ssize_t pty_master_read(struct chrdev *dev, void *buf, size_t count, size_t off);
static ssize_t pty_slave_write(struct chrdev *dev, const void *buf, size_t count, size_t off);
static ssize_t pty_slave_read(struct chrdev *dev, void *buf, size_t count, size_t off);

//static struct chrdev pty_master_dev = {
//    .read = pty_master_read,
//    .write = pty_master_write
//};
//
//static struct chrdev pty_slave_dev = {
//    .read = pty_slave_read,
//    .write = pty_slave_write
//};

static ssize_t pty_slave_write(struct chrdev *dev, const void *buf, size_t off, size_t count) {
    // Slave writes -> pty output -> Master reads
    // AS-IS
    struct pty *pty = dev->dev_data;
    _assert(pty);
    int res = 0;

    for (size_t i = 0; i < count; ++i) {
        if ((res = ring_putc(&pty->ring_output, ((const char *) buf)[i])) < 0) {
            break;
        }
    }

    return (res < 0) ? res : (ssize_t) count;
}

static ssize_t pty_slave_read(struct chrdev *dev, void *buf, size_t off, size_t count) {
    // Slave reads <- pty input <- Master writes
    // TODO: process stuff like ^C/^D etc.
    struct pty *pty = dev->dev_data;
    _assert(pty);
    char ibuf[16];
    size_t rem = count;
    size_t p = 0;

    if (count == 0) {
        return 0;
    }

    while (rem) {
        ssize_t rd = ring_read(get_cpu()->thread, &pty->ring_output, ibuf, MIN(16, rem));
        if (rd < 0) {
            break;
        }
        memcpy((char *) buf + p, ibuf, rd);

        rem -= rd;
    }

    return p;
}

static ssize_t pty_master_write(struct chrdev *dev, const void *buf, size_t pos, size_t count) {
    // Master writes -> pty input -> Slave reads
    // TODO: process stuff like CR/LF conversion etc.
    struct pty *pty = dev->dev_data;
    _assert(pty);
    int res = 0;

    for (size_t i = 0; i < count; ++i) {
        if ((res = ring_putc(&pty->ring_input, ((const char *) buf)[i])) < 0) {
            break;
        }
    }

    return (res < 0) ? res : (ssize_t) count;
}

static ssize_t pty_master_read(struct chrdev *dev, void *buf, size_t off, size_t count) {
    // Master reads <- pty output <- Slave writes
    // AS-IS
    struct pty *pty = dev->dev_data;
    _assert(pty);
    char ibuf[16];
    size_t rem = count;
    size_t p = 0;

    if (count == 0) {
        return 0;
    }

    while (rem) {
        ssize_t rd = ring_read(get_cpu()->thread, &pty->ring_output, ibuf, MIN(16, rem));
        if (rd < 0) {
            break;
        }
        memcpy((char *) buf + p, ibuf, rd);

        rem -= rd;
    }

    return p;
}

struct pty *pty_create(void) {
    struct pty *pty = kmalloc(sizeof(struct pty));
    _assert(pty);

    ring_init(&pty->ring_input, PTY_RING_SIZE);
    ring_init(&pty->ring_output, PTY_RING_SIZE);

    pty->dev_master.read = pty_master_read;
    pty->dev_master.write = pty_master_write;
    pty->dev_master.dev_data = pty;
    pty->dev_slave.read = pty_slave_read;
    pty->dev_slave.write = pty_slave_write;
    pty->dev_slave.dev_data = pty;

    vnode_t *pty_master = kmalloc(sizeof(vnode_t));
    _assert(pty_master);
    vnode_t *pty_slave = kmalloc(sizeof(vnode_t));
    _assert(pty_slave);

    // TODO: add these to devfs
    pty_master->refcount = 0;
    pty_master->fs = NULL;
    pty_master->tree_node = NULL;
    pty_master->type = VN_CHR;
    pty_master->dev = &pty->dev_master;

    pty_slave->refcount = 0;
    pty_slave->fs = NULL;
    pty_slave->tree_node = NULL;
    pty_slave->type = VN_CHR;
    pty_slave->dev = &pty->dev_slave;

    pty->master = pty_master;
    pty->slave = pty_slave;

    return pty;
}
