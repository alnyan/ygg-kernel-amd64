#include "bits/syscall.h"
#include "bits/global.h"
#include <errno.h>

#define ASM_REGISTER(name) \
        register uint64_t name asm (#name)

#define ASM_SYSCALL0(r0)                    ({ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        asm volatile ("syscall":::"memory"); \
        rax; \
    })

#define ASM_SYSCALL1(r0, r1)                ({ \
        ASM_REGISTER(rdi) = (uint64_t) (r1); \
        /*
         * Should be the last one because memory accesses for arguments
         * fuck up %rax
         */ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        asm volatile ("syscall":::"memory"); \
        rax; \
    })

#define ASM_SYSCALL2(r0, r1, r2)        ({ \
        ASM_REGISTER(rdi) = (uint64_t) (r1); \
        ASM_REGISTER(rsi) = (uint64_t) (r2); \
        /*
         * Should be the last one because memory accesses for arguments
         * fuck up %rax
         */ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        asm volatile ("syscall":::"memory", "rax", "rdi", "rsi", "rdx"); \
        rax; \
    })

#define ASM_SYSCALL3(r0, r1, r2, r3)        ({ \
        ASM_REGISTER(rdi) = (uint64_t) (r1); \
        ASM_REGISTER(rsi) = (uint64_t) (r2); \
        ASM_REGISTER(rdx) = (uint64_t) (r3); \
        /*
         * Should be the last one because memory accesses for arguments
         * fuck up %rax
         */ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        asm volatile ("syscall":::"memory", "rax", "rdi", "rsi", "rdx"); \
        rax; \
    })

#define ASM_SYSCALL4(r0, r1, r2, r3, r4)        ({ \
        ASM_REGISTER(rdi) = (uint64_t) (r1); \
        ASM_REGISTER(rsi) = (uint64_t) (r2); \
        ASM_REGISTER(rdx) = (uint64_t) (r3); \
        ASM_REGISTER(r10) = (uint64_t) (r4); \
        /*
         * Should be the last one because memory accesses for arguments
         * fuck up %rax
         */ \
        ASM_REGISTER(rax) = (uint64_t) (r0); \
        asm volatile ("syscall":::"memory", "rax", "rdi", "rsi", "rdx", "r10"); \
        rax; \
    })

#define SET_ERRNO(t, r)                     ({ \
        t res = (t) r; \
        if (res < 0) { \
            errno = -res; \
        } \
        ((int) res) < 0 ? (t) -1 : res; \
    })

ssize_t write(int fd, const void *buf, size_t count) {
    return SET_ERRNO(ssize_t, ASM_SYSCALL3(SYSCALL_NR_WRITE, fd, buf, count));
}

ssize_t read(int fd, void *buf, size_t count) {
    return SET_ERRNO(ssize_t, ASM_SYSCALL3(SYSCALL_NR_READ, fd, buf, count));
}

ssize_t sys_readdir(int fd, struct dirent *entp) {
    return SET_ERRNO(ssize_t, ASM_SYSCALL2(SYSCALL_NR_READDIR, fd, entp));
}

int open(const char *filename, int flags, int mode) {
    return SET_ERRNO(int, ASM_SYSCALL3(SYSCALL_NR_OPEN, filename, flags, mode));
}

void close(int fd) {
    (void) ASM_SYSCALL1(SYSCALL_NR_CLOSE, fd);
}

int stat(const char *filename, struct stat *st) {
    return SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NR_STAT, filename, st));
}

int brk(void *addr) {
    return SET_ERRNO(int, ASM_SYSCALL1(SYSCALL_NR_BRK, addr));
}

int fork(void) {
    return SET_ERRNO(int, ASM_SYSCALL0(SYSCALL_NR_FORK));
}

int execve(const char *filename, const char *const argv[], const char *const envp[]) {
    return SET_ERRNO(int, ASM_SYSCALL3(SYSCALL_NR_EXECVE, filename, argv, envp));
}

__attribute__((noreturn)) void exit(int code) {
    (void) ASM_SYSCALL1(SYSCALL_NR_EXIT, code);
    while (1);
}

int kill(int pid, int signum) {
    return SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NR_KILL, pid, signum));
}

void __kernel_signal(uintptr_t entry) {
    (void) ASM_SYSCALL1(SYSCALL_NRX_SIGNAL, entry);
}

__attribute__((noreturn)) void __kernel_sigret(void) {
    (void) ASM_SYSCALL0(SYSCALL_NRX_SIGRET);
    while (1);
}

int getpid(void) {
    return ASM_SYSCALL0(SYSCALL_NR_GETPID);
}

int chdir(const char *filename) {
    return SET_ERRNO(int, ASM_SYSCALL1(SYSCALL_NR_CHDIR, filename));
}

char *getcwd(char *buf, size_t lim) {
    if (SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NR_GETCWD, buf, lim)) == 0) {
        return buf;
    } else {
        return NULL;
    }
}

int nanosleep(const struct timespec *req, struct timespec *rem) {
    return SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NR_NANOSLEEP, req, rem));
}

int openpty(int *master, int *slave) {
    return SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NRX_OPENPTY, master, slave));
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    return SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NR_GETTIMEOFDAY, tv, tz));
}

int reboot(int magic1, int magic2, unsigned int cmd, void *arg) {
    return SET_ERRNO(int, ASM_SYSCALL4(SYSCALL_NR_REBOOT, magic1, magic2, cmd, arg));
}

int access(const char *filename, int mode) {
    return SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NR_ACCESS, filename, mode));
}

int waitpid(int pid, int *status) {
    return SET_ERRNO(int, ASM_SYSCALL2(SYSCALL_NRX_WAITPID, pid, status));
}

// Although sbrk() is implemented in userspace, I guess it should also be here
void *sbrk(intptr_t inc) {
    if (inc == 0) {
        return __cur_brk;
    }

    void *new_brk = (void *) ((uintptr_t) __cur_brk + inc);

    if (brk(new_brk) != 0) {
        return __cur_brk;
    } else {
        return __cur_brk = new_brk;
    }
}
