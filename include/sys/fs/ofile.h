#pragma once
#include "node.h"
#include <sys/types.h>

struct ofile {
    int flags;
    struct vnode *vnode;
    size_t pos;
};
