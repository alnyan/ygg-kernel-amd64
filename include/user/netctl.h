#pragma once

#define NETCTL_SET_INADDR       0x0001
#define NETCTL_GET_INADDR       0x0002

#if !defined(__KERNEL__)
#include <stdint.h>
int netctl(const char *iface, uint32_t cmd, void *arg);
#endif
