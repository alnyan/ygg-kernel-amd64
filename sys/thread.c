#include "sys/thread.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/debug.h"
#include "sys/heap.h"

void thread_ioctx_fork(struct thread *dst, struct thread *src) {
    for (size_t i = 0; i < THREAD_MAX_FDS; ++i) {
        dst->fds[i] = src->fds[i];
        if (dst->fds[i]) {
            ++dst->fds[i]->refcount;
        }
    }

    // Clone ioctx: cwd vnode reference and path
    dst->ioctx.cwd_vnode = src->ioctx.cwd_vnode;
    dst->ioctx.uid = src->ioctx.uid;
    dst->ioctx.gid = src->ioctx.gid;
}

void thread_ioctx_cleanup(struct thread *t) {
    for (size_t i = 0; i < THREAD_MAX_FDS; ++i) {
        if (t->fds[i]) {
            vfs_close(&t->ioctx, t->fds[i]);

            _assert(t->fds[i]->refcount >= 0);
            if (t->fds[i]->refcount == 0) {
                kdebug("No one uses fd %d, freeing it\n", i);
                kfree(t->fds[i]);
            }
        }
    }
}

void thread_signal(struct thread *t, int s) {
    _assert(t);
    _assert(s > 0 && s <= 32);
    t->sigq |= (1 << (s - 1));
}

void thread_set_name(struct thread *t, const char *name) {
    if (name) {
        _assert(strlen(name) < 32);
        strcpy(t->name, name);
    } else {
        t->name[0] = 0;
    }
}

