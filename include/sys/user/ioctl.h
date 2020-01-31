#pragma once

#if !defined(__KERNEL__)
int ioctl(int fd, unsigned int cmd, void *arg);
#endif
