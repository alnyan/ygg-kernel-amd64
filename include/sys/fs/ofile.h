#pragma once
#include "node.h"
#include <sys/types.h>

#define OF_WRITABLE         (1 << 0)
#define OF_READABLE         (1 << 1)
#define OF_DIRECTORY        (1 << 2)
#define OF_MEMDIR           (1 << 3)
#define OF_MEMDIR_DOT       (1 << 4)
#define OF_MEMDIR_DOTDOT    (1 << 5)

struct ofile {
    int refcount;
    int flags;
    struct vnode *vnode;
    size_t pos;
    void *priv_data;
};
