#pragma once
#include "sys/types.h"

struct tcp_frame {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack_seq;
    uint8_t ns:1;
    uint8_t zero:3;
    uint8_t doff:4;
    uint8_t fin:1;
    uint8_t syn:1;
    uint8_t rst:1;
    uint8_t psh:1;
    uint8_t ack:1;
    uint8_t urg:1;
    uint8_t ece:1;
    uint8_t cwr:1;
    uint16_t window;
    uint16_t checksum;
    uint16_t urg_ptr;
} __attribute__((packed));

struct eth_frame;
struct inet_frame;
struct packet;
struct vfs_ioctx;
struct ofile;
struct sockaddr;

void tcp_handle_frame(struct packet *p, struct eth_frame *eth, struct inet_frame *ip, void *data, size_t len);
