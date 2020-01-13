#pragma once
#include "sys/types.h"
#include "sys/time.h"

int sys_gettimeofday(struct timeval *tv, struct timezone *tz);
int sys_nanosleep(const struct timespec *req, struct timespec *rem);
int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg);
int sys_mount(const char *dev_name, const char *dir_name, const char *type, unsigned long flags, void *data);
