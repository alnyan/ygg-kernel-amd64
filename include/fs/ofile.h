#pragma once
#include "node.h"
#include "net/socket.h"
#include <sys/types.h>

#define OF_WRITABLE         (1 << 0)
#define OF_READABLE         (1 << 1)
#define OF_DIRECTORY        (1 << 2)
#define OF_MEMDIR           (1 << 3)
#define OF_MEMDIR_DOT       (1 << 4)
#define OF_MEMDIR_DOTDOT    (1 << 5)
#define OF_SOCKET           (1 << 6)

struct ofile {
    int flags;
    union {
        struct {
            int refcount;
            struct vnode *vnode;
            size_t pos;
            void *priv_data;
        } file;
        struct socket socket;
    };
};
