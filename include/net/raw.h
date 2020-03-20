#pragma once
#include "sys/types.h"

struct vfs_ioctx;
struct ofile;
struct sockaddr;
struct packet;

int raw_socket_open(struct vfs_ioctx *ioctx, struct ofile *fd, int dom, int type, int proto);
ssize_t raw_socket_send(struct vfs_ioctx *ioctx, struct ofile *fd, const void *buf, size_t lim, struct sockaddr *sa, size_t salen);
ssize_t raw_socket_recv(struct vfs_ioctx *ioctx, struct ofile *fd, void *buf, size_t lim, struct sockaddr *sa, size_t *salen);
int raw_socket_bind(struct vfs_ioctx *ioctx, struct ofile *fd, struct sockaddr *sa, size_t len);
int raw_setsockopt(struct vfs_ioctx *ioctx, struct ofile *fd, int optname, void *optval, size_t optlen);
void raw_socket_close(struct vfs_ioctx *ioctx, struct ofile *fd);

void raw_packet_handle(struct packet *p);
