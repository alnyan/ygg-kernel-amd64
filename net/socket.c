#include "user/socket.h"
#include "user/errno.h"
#include "sys/assert.h"
#include "user/inet.h"
#include "sys/debug.h"
#include "net/class.h"
#include "fs/ofile.h"

static LIST_HEAD(g_socket_class_head);

void socket_class_register(struct socket_class *cls) {
    list_head_init(&cls->link);
    list_add(&cls->link, &g_socket_class_head);
}

int net_open(struct vfs_ioctx *ioctx, struct ofile *fd, int dom, int type, int proto) {
    struct socket_class *cls, *iter;

    cls = NULL;
    list_for_each_entry(iter, &g_socket_class_head, link) {
        if (iter->domain == dom && iter->type == type) {
            _assert(iter->supports);

            if (iter->supports(proto)) {
                cls = iter;
                break;
            }
        }
    }

    if (!cls) {
        // No support for (dom:type:proto) tuple
        return -EINVAL;
    }

    _assert(cls->ops && cls->ops->open);

    fd->flags = OF_SOCKET;
    fd->socket.ioctx = ioctx;
    fd->socket.op = cls->ops;

    return cls->ops->open(&fd->socket);
}

void net_close(struct vfs_ioctx *ioctx, struct ofile *fd) {
    _assert(fd);
    _assert(fd->flags & OF_SOCKET);
    _assert(fd->socket.ioctx == ioctx);
    _assert(fd->socket.op);

    if (fd->socket.op->close) {
        fd->socket.op->close(&fd->socket);
    }
    fd->flags &= ~OF_SOCKET;
}

ssize_t net_sendto(struct vfs_ioctx *ioctx,
                   struct ofile *fd,
                   const void *buf,
                   size_t len,
                   struct sockaddr *sa,
                   size_t salen) {
    _assert(fd);
    _assert(fd->flags & OF_SOCKET);
    _assert(fd->socket.ioctx == ioctx);
    _assert(fd->socket.op);

    if (fd->socket.op->sendto) {
        return fd->socket.op->sendto(&fd->socket, buf, len, sa, salen);
    } else {
        return -EINVAL;
    }
}

ssize_t net_recvfrom(struct vfs_ioctx *ioctx,
                     struct ofile *fd,
                     void *buf,
                     size_t len,
                     struct sockaddr *sa,
                     size_t *salen) {
    _assert(fd);
    _assert(fd->flags & OF_SOCKET);
    _assert(fd->socket.ioctx == ioctx);
    _assert(fd->socket.op);

    if (fd->socket.op->recvfrom) {
        return fd->socket.op->recvfrom(&fd->socket, buf, len, sa, salen);
    } else {
        return -EINVAL;
    }
}

int net_bind(struct vfs_ioctx *ioctx, struct ofile *fd, struct sockaddr *sa, size_t len) {
    _assert(fd);
    _assert(fd->flags & OF_SOCKET);
    _assert(fd->socket.ioctx == ioctx);
    _assert(fd->socket.op);

    if (fd->socket.op->bind) {
        return fd->socket.op->bind(&fd->socket, sa, len);
    } else {
        return -EINVAL;
    }
}

int net_setsockopt(struct vfs_ioctx *ioctx, struct ofile *fd, int optname, void *optval, size_t optlen) {
    _assert(fd);
    _assert(fd->flags & OF_SOCKET);
    _assert(fd->socket.ioctx == ioctx);
    _assert(fd->socket.op);

    if (fd->socket.op->setsockopt) {
        return fd->socket.op->setsockopt(&fd->socket, optname, optval, optlen);
    } else {
        return -EINVAL;
    }
}
