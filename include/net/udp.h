#pragma once
#include "sys/types.h"

struct udp_frame {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
};

struct eth_frame;
struct inet_frame;
struct packet;
struct vfs_ioctx;
struct ofile;
struct sockaddr;

void udp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *data, size_t len);
int udp_socket_open(struct vfs_ioctx *ioctx, struct ofile *fd, int dom, int type, int proto);
ssize_t udp_socket_send(struct vfs_ioctx *ioctx, struct ofile *fd, const void *buf, size_t lim, struct sockaddr *sa, size_t salen);
