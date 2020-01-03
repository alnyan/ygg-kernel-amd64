#pragma once
#include "sys/fs/node.h"
#include "sys/ring.h"
#include "sys/chr.h"

struct pty {
    int number;
    vnode_t *master;
    vnode_t *slave;
    // TODO: winsize/termios

    struct chrdev dev_master;
    struct chrdev dev_slave;

    // Slave reads
    // Master writes
    struct ring ring_input;
    // Slave writes
    // Master reads
    struct ring ring_output;
    int owner_pid;
};

struct pty *pty_create(void);
