#pragma once
#include <sys/fs/dirent.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int open(const char *filename, int flags, int mode);
void close(int fd);
int stat(const char *filename, struct stat *st);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, uintptr_t offset);
int fork(void);
int execve(const char *filename, const char *const argv[], const char *const envp[]);
ssize_t sys_readdir(int fd, struct dirent *entp);
int kill(int pid, int signum);
void __kernel_signal(uintptr_t handler);
__attribute__((noreturn)) void __kernel_sigret(void);
int getpid(void);

__attribute__((noreturn)) void exit(int code);
