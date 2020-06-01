#pragma once
#include "sys/types.h"

struct netdev;

int net_receive(struct netdev *dev, const void *data, size_t len);
void net_init(void);
void net_daemon_start(void);
