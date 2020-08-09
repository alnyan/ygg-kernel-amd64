#pragma once

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#define R_OK        4
#define W_OK        2
#define X_OK        1
#define F_OK        0

#define AT_FDCWD                (-100)
#define AT_SYMLINK_NOFOLLOW     (1 << 0)
#define AT_EMPTY_PATH           (1 << 1)
#define AT_REMOVEDIR            (1 << 2)

// O_EXEC is a special one for opening a node for
//        execution
#define O_EXEC          (1 << 2)

#define O_ACCMODE       (3 << 0)
#define O_RDONLY        (0 << 0)
#define O_WRONLY        (1 << 0)
#define O_RDWR          (2 << 0)
#define O_CREAT         (1 << 6)
#define O_EXCL          (1 << 7)
#define O_TRUNC         (1 << 9)
#define O_APPEND        (1 << 10)
#define O_DIRECTORY     (1 << 16)
#define O_CLOEXEC       (1 << 19)
// #define O_NOCTTY        00000400
// #define O_NONBLOCK      00004000
// #define O_DSYNC         00010000
// #define FASYNC          00020000
// #define O_DIRECT        00040000
// #define O_LARGEFILE     00100000
// #define O_NOFOLLOW      00400000
// #define O_NOATIME       01000000

#define FD_CLOEXEC      (1 << 0)

// fcntl() commands
#define F_GETFD         1
#define F_SETFD         2
