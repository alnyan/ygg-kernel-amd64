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
    struct process *proc = thread_self->proc;
    int fd = -1;
    int res;

    // XXX: This should be atomic
    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (!proc->fds[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        return -EMFILE;
    }

    struct ofile *ofile = ofile_create();

    if ((res = net_open(&proc->ioctx, ofile, domain, type, protocol)) != 0) {
        ofile_destroy(ofile);
        return res;
    }

    proc->fds[fd] = ofile_dup(ofile);
    return fd;
}

ssize_t sys_sendto(int fd, const void *buf, size_t len, struct sockaddr *sa, size_t salen) {
    struct process *proc = thread_self->proc;
    struct ofile *of;

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = proc->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_sendto(&proc->ioctx, of, buf, len, sa, salen);
}

ssize_t sys_recvfrom(int fd, void *buf, size_t len, struct sockaddr *sa, size_t *salen) {
    struct process *proc = thread_self->proc;
    struct ofile *of;

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = proc->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_recvfrom(&proc->ioctx, of, buf, len, sa, salen);
}

int sys_bind(int fd, struct sockaddr *sa, size_t salen) {
    struct process *proc = thread_self->proc;
    struct ofile *of;

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = proc->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_bind(&proc->ioctx, of, sa, salen);
}

int sys_connect(int fd, struct sockaddr *sa, size_t salen) {
    struct process *proc = thread_self->proc;
    struct ofile *of;

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = proc->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_connect(&proc->ioctx, of, sa, salen);
}

int sys_accept(int fd, struct sockaddr *sa, size_t *salen) {
    struct process *proc = thread_self->proc;
    struct ofile *of;
    int res, client_fd = -1;

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = proc->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    struct ofile *client_ofile = NULL;
    if ((res = net_accept(&proc->ioctx, of, &client_ofile, sa, salen)) != 0) {
        return res;
    }
    _assert(client_ofile);
    // XXX: This should be atomic
    for (int i = 0; i < THREAD_MAX_FDS; ++i) {
        if (!proc->fds[i]) {
            client_fd = i;
            break;
        }
    }
    if (client_fd == -1) {
        return -EMFILE;
    }
    proc->fds[client_fd] = ofile_dup(client_ofile);

    return client_fd;
}

int sys_setsockopt(int fd, int level, int optname, void *optval, size_t optlen) {
    struct process *proc = thread_self->proc;
    struct ofile *of;

    // XXX: level is ignored (only 1 is used)

    if (fd < 0 || fd >= THREAD_MAX_FDS) {
        return -EBADF;
    }

    if ((of = proc->fds[fd]) == NULL) {
        return -EBADF;
    }

    if (!(of->flags & OF_SOCKET)) {
        kwarn("Invalid operation on non-socket\n");
        return -EINVAL;
    }

    return net_setsockopt(&proc->ioctx, of, optname, optval, optlen);
}
