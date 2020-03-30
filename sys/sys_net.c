#include "arch/amd64/cpu.h"
#include "sys/sys_net.h"
#include "sys/assert.h"
#include "sys/thread.h"
#include "net/socket.h"
#include "user/errno.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "fs/ofile.h"
#include "net/net.h"
#include "net/if.h"

int sys_netctl(const char *name, uint32_t op, void *arg) {
    _assert(name);

    struct netdev *dev = netdev_by_name(name);
    if (!dev) {
        return -ENODEV;
    }

    return netctl(dev, op, arg);
}

int sys_socket(int domain, int type, int protocol) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);
    int fd = -1;
    int res;

    // XXX: This should be atomic
    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (!thr->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        return -EMFILE;
    }

    struct ofile *ofile = kmalloc(sizeof(struct ofile));
    _assert(ofile);

    if ((res = net_open(&thr->ioctx, ofile, domain, type, protocol)) != 0) {
        kfree(ofile);
        return res;
    }

    thr->fds[fd] = ofile;
    return fd;
}

ssize_t sys_sendto(int fd, const void *buf, size_t len, struct sockaddr *sa, size_t salen) {
    struct thread *thr = thread_self;
    struct ofile *of;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = thr->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_sendto(&thr->ioctx, of, buf, len, sa, salen);
}

ssize_t sys_recvfrom(int fd, void *buf, size_t len, struct sockaddr *sa, size_t *salen) {
    struct thread *thr = thread_self;
    struct ofile *of;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = thr->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_recvfrom(&thr->ioctx, of, buf, len, sa, salen);
}

int sys_bind(int fd, struct sockaddr *sa, size_t salen) {
    struct thread *thr = thread_self;
    struct ofile *of;
    _assert(thr);

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = thr->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_bind(&thr->ioctx, of, sa, salen);
}

int sys_setsockopt(int fd, int level, int optname, void *optval, size_t optlen) {
    struct thread *thr = thread_self;
    struct ofile *of;
    _assert(thr);

    // XXX: level is ignored (only 1 is used)

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = thr->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_setsockopt(&thr->ioctx, of, optname, optval, optlen);
}
