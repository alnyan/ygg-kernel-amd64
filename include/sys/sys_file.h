#pragma once
#include "user/select.h"
#include "sys/types.h"

ssize_t sys_read(int fd, void *data, size_t lim);
ssize_t sys_write(int fd, const void *data, size_t lim);
int sys_ftruncate(int fd, off_t length);
int sys_truncate(const char *filename, off_t length);
int sys_creat(const char *pathname, int mode);
int sys_mkdirat(int dfd, const char *pathname, int mode);
int sys_unlinkat(int dfd, const char *pathname, int flags);
ssize_t sys_readdir(int fd, struct dirent *ent);
int sys_chdir(const char *filename);
int sys_ioctl(int fd, unsigned int cmd, void *arg);
int sys_getcwd(char *buf, size_t lim);
int sys_openat(int dfd, const char *filename, int flags, int mode);
void sys_close(int fd);
int sys_fstatat(int dfd, const char *pathname, struct stat *st, int flags);
int sys_faccessat(int dfd, const char *pathname, int mode, int flags);
int sys_pipe(int *filedes);
int sys_select(int n, fd_set *inp, fd_set *outp, fd_set *excp, struct timeval *tv);
int sys_dup(int from);
int sys_dup2(int from, int to);
int sys_chmod(const char *path, mode_t mode);
int sys_chown(const char *path, uid_t uid, gid_t gid);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_mknod(const char *filename, int mode, unsigned int dev);
ssize_t sys_readlinkat(int dfd, const char *restrict pathname, char *restrict buf, size_t lim);
