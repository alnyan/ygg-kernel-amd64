#pragma once
#include "sys/types.h"

int net_receive(/* TODO: replace with struct netdev * */ void *ctx, const void *data, size_t len);

int net_daemon_start(void);
