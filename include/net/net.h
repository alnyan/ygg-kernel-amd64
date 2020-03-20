#pragma once
#include "sys/types.h"

struct sockaddr;
struct vfs_ioctx;
struct ofile;
struct netdev;

int net_receive(struct netdev *dev, const void *data, size_t len);
int net_socket_open(struct vfs_ioctx *ioctx, struct ofile *fd, int dom, int type, int proto);
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

void net_daemon_start(void);
