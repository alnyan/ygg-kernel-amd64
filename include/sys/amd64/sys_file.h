#pragma once
#include "sys/types.h"
#include "sys/fs/dirent.h"
#include "sys/stat.h"
#include "sys/select.h"

struct timeval;

ssize_t sys_read(int fd, void *buf, size_t lim);
ssize_t sys_write(int fd, const void *buf, size_t lim);
int sys_open(const char *filename, int flags, int mode);
void sys_close(int fd);
int sys_creat(const char *pathname, int mode);

ssize_t sys_readdir(int fd, struct dirent *ent);
int sys_getcwd(char *buf, size_t lim);
int sys_chdir(const char *filename);

int sys_mkdir(const char *pathname, int mode);
int sys_unlink(const char *pathname);
int sys_rmdir(const char *pathname);

int sys_stat(const char *filename, struct stat *st);
int sys_access(const char *path, int mode);

// XXX: Except timeout is const, as I don't want to modify it
//      No one cares about how much time is remaining once
//      select() returns, right?
int sys_select(int nfds, fd_set *rd, fd_set *wr, fd_set *exc, struct timeval *timeout);

// TODO: const struct termios *termp, const struct winsize *winp
int sys_openpty(int *master, int *slave);

int sys_chmod(const char *path, mode_t mode);
int sys_chown(const char *path, uid_t uid, gid_t gid);
off_t sys_lseek(int fd, off_t offset, int whence);

// XXX: Will be removed once ioctl(fd, TCGETS, ...) is possible
int sys_isatty(int fd);
