/** vi: ft=c.doxygen :
 * @file statvfs.h
 * @brief Filesystem status information
 */
#pragma once
#include <stdint.h>

typedef uint64_t fsblkcnt_t;

struct statvfs {
    uint64_t f_bsize;
    uint64_t f_frsize;
    fsblkcnt_t f_blocks;
    fsblkcnt_t f_bfree;
    fsblkcnt_t f_bavail;
    fsblkcnt_t f_files;
    fsblkcnt_t f_ffree;
    fsblkcnt_t f_favail;
    uint64_t f_fsid;
    uint64_t f_flag;
    uint64_t f_namemax;
};
