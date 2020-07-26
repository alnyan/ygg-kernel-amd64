#pragma once
#if defined(__KERNEL__)
#include <stdint.h>
#else
#include <stdint-gcc.h>
#endif

typedef int sig_atomic_t;
typedef int pid_t;

typedef int64_t ssize_t;
typedef uint64_t size_t;

typedef int64_t off_t;
typedef int mode_t;

typedef uint32_t uid_t;
typedef uint32_t gid_t;

typedef uint64_t clock_t;
typedef uint64_t useconds_t;
