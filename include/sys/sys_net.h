#pragma once
#include "sys/types.h"

struct sockaddr;

int sys_netctl(const char *name, uint32_t op, void *arg);
int sys_socket(int domain, int type, int protocol);
ssize_t sys_sendto(int fd, const void *buf, size_t len, struct sockaddr *sa, size_t salen);
