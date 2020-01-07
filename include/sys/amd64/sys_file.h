#pragma once
#include "sys/types.h"
#include "sys/fs/dirent.h"
#include "sys/stat.h"

ssize_t sys_read(int fd, void *buf, size_t lim);
ssize_t sys_write(int fd, const void *buf, size_t lim);
int sys_open(const char *filename, int flags, int mode);
void sys_close(int fd);

ssize_t sys_readdir(int fd, struct dirent *ent);
int sys_getcwd(char *buf, size_t lim);
int sys_chdir(const char *filename);

int sys_stat(const char *filename, struct stat *st);
int sys_access(const char *path, int mode);

// TODO: const struct termios *termp, const struct winsize *winp
int sys_openpty(int *master, int *slave);
