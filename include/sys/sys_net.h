#pragma once
#include "sys/types.h"

int sys_netctl(const char *name, uint32_t op, void *arg);
