#pragma once

struct utsname;
struct timespec;
struct timeval;
struct timezone;

int sys_mount(const char *dev_name, const char *dir_name, const char *type, unsigned long flags, void *data);
int sys_umount(const char *target);
int sys_uname(struct utsname *name);
int sys_nanosleep(const struct timespec *req, struct timespec *rem);
int sys_gettimeofday(struct timeval *tv, struct timezone *tz);
int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg);
