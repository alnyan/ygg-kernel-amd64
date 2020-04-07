#pragma once

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#define R_OK        4
#define W_OK        2
#define X_OK        1
#define F_OK        0


// O_EXEC is a special one for opening a node for
//        execution
#define O_EXEC      (1 << 2)

#define O_ACCMODE       00000003
#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_RDWR          00000002
#define O_CREAT         00000100
// #define O_EXCL          00000200
// #define O_NOCTTY        00000400
#define O_TRUNC         00001000
#define O_APPEND        00002000
// #define O_NONBLOCK      00004000
// #define O_DSYNC         00010000
// #define FASYNC          00020000
// #define O_DIRECT        00040000
// #define O_LARGEFILE     00100000
//#define O_DIRECTORY     00200000
#define O_DIRECTORY     0x200000
// #define O_NOFOLLOW      00400000
// #define O_NOATIME       01000000
// #define O_CLOEXEC       02000000

