#pragma once
#include <stdint-gcc.h>

#if !defined(NULL)
#define NULL        ((void *) 0)
#endif

typedef int sig_atomic_t;

typedef int32_t pid_t;

typedef int64_t ssize_t;
typedef uint64_t size_t;
typedef int64_t off_t;

typedef uint32_t mode_t;

typedef uint16_t uid_t;
typedef uint16_t gid_t;

typedef uint64_t clock_t;
typedef uint64_t useconds_t;

typedef int32_t pid_t;
