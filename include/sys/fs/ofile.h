#pragma once
#include "node.h"
#include <sys/types.h>

#define OF_WRITABLE         (1 << 0)
#define OF_READABLE         (1 << 1)
#define OF_DIRECTORY        (1 << 2)

struct ofile {
    int flags;
    struct vnode *vnode;
    size_t pos;
};
