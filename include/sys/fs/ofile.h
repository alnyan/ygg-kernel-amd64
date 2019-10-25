#pragma once
#include "node.h"
#include <sys/types.h>

struct ofile {
    int flags;
    vnode_t *vnode;
    size_t pos;
    // Dirent buffer
    char dirent_buf[512];
};
