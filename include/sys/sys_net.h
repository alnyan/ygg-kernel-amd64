#pragma once
#include "sys/types.h"

struct sockaddr;

int sys_netctl(const char *name, uint32_t op, void *arg);
int sys_socket(int domain, int type, int protocol);
ssize_t sys_sendto(int fd, const void *buf, size_t len, struct sockaddr *sa, size_t salen);
ssize_t sys_recvfrom(int fd, void *buf, size_t len, struct sockaddr *sa, size_t *salen);
int sys_bind(int fd, struct sockaddr *sa, size_t len);
int sys_connect(int fd, struct sockaddr *sa, size_t len);
int sys_setsockopt(int fd, int level, int optname, void *optval, size_t len);
int sys_accept(int fd, struct sockaddr *sa, size_t *salen);
