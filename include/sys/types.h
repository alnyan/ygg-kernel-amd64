#pragma once
#include <stdint.h>

#ifndef NULL
#define NULL ((void *) 0)
#endif

#if defined(__KERNEL__)
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
typedef uint32_t uid_t;
typedef uint32_t gid_t;
#endif

typedef uint64_t uintptr_t;
typedef int64_t intptr_t;

typedef uintptr_t size_t;
typedef intptr_t ssize_t;

typedef int64_t off_t;

typedef uint32_t mode_t;

typedef int32_t pid_t;
