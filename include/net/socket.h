#pragma once
#include "sys/types.h"
#include "sys/wait.h"

struct sockaddr;
struct vfs_ioctx;
struct ofile;
struct netdev;

struct socket {
    struct sockops *op;
    struct vfs_ioctx *ioctx;
    struct io_notify rx_notify;
    void *data;
};

int net_open(struct vfs_ioctx *ioctx, struct ofile *fd, int dom, int type, int proto);
ssize_t net_sendto(struct vfs_ioctx *ioctx,
                   struct ofile *fd,
                   const void *buf,
                   size_t len,
                   struct sockaddr *sa,
                   size_t salen);
ssize_t net_recvfrom(struct vfs_ioctx *ioctx,
                     struct ofile *fd,
                     void *buf,
                     size_t len,
                     struct sockaddr *sa,
                     size_t *salen);
int net_bind(struct vfs_ioctx *ioctx, struct ofile *fd, struct sockaddr *sa, size_t len);
int net_setsockopt(struct vfs_ioctx *ioctx, struct ofile *fd, int optname, void *optval, size_t optlen);
void net_close(struct vfs_ioctx *ioctx, struct ofile *fd);

int socket_has_data(struct socket *sock);
