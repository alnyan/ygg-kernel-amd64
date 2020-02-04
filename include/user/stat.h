/** vi: ft=c.doxygen :
 * @file stat.h
 * @brief File status structure
 */
#pragma once
#if defined(__KERNEL__)
#include "sys/types.h"
#else
#include <ygg/types.h>
#endif

#define S_IFMT          0170000
#define S_IFSOCK        0140000
#define S_IFLNK         0120000
#define S_IFREG         0100000
#define S_IFBLK         0060000
#define S_IFDIR         0040000
#define S_IFCHR         0020000
#define S_IFIFO         0010000

#define S_ISSOCK(m)     ((m & S_IFMT) == S_IFSOCK)
#define S_ISLNK(m)      ((m & S_IFMT) == S_IFLNK)
#define S_ISREG(m)      ((m & S_IFMT) == S_IFREG)
#define S_ISBLK(m)      ((m & S_IFMT) == S_IFBLK)
#define S_ISDIR(m)      ((m & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)      ((m & S_IFMT) == S_IFCHR)
#define S_ISFIFO(m)     ((m & S_IFMT) == S_IFIFO)

#define S_ISUID         04000
#define S_ISGID         02000
#define S_ISVTX         01000

#define S_IRUSR         00400
#define S_IWUSR         00200
#define S_IXUSR         00100
#define S_IRWXU         (S_IRUSR | S_IWUSR | S_IXUSR)

#define S_IRGRP         00040
#define S_IWGRP         00020
#define S_IXGRP         00010
#define S_IRWXG         (S_IRGRP | S_IWGRP | S_IXGRP)

#define S_IROTH         00004
#define S_IWOTH         00002
#define S_IXOTH         00001
#define S_IRWXO         (S_IROTH | S_IWOTH | S_IXOTH)

struct stat {
    uint32_t st_dev;
    uint32_t st_ino;
    mode_t st_mode;
    uint32_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    uint32_t st_rdev;
    uint32_t st_size;
    uint32_t st_blksize;
    uint32_t st_blocks;
    uint32_t st_atime;
    uint32_t st_mtime;
    uint32_t st_ctime;
};
