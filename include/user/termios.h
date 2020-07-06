#pragma once

#define TCGETS          0x5401
#define TCSETS          0x5402
#define TIOCSPGRP       0x5410
#define TIOCGWINSZ      0x5413

// Input flags
#define ICANON          (1 << 1)

// Output flags
#define OPOST           (1 << 0)
#define ONLCR           (1 << 2)
#define OCRNL           (1 << 4)
#define ONLRET          (1 << 6)

// Control flags
#define ISIG            (1 << 0)
#define ECHO            (1 << 4)        // Echo input characters
#define ECHOE           (1 << 5)        // if ICANON, ERASE erases prec. character,
                                        //            WERASE erases prec. word
#define ECHOK           (1 << 6)        // if ICANON, KILL character erases the line
#define ECHONL          (1 << 8)        // if ICANON, echo newline (even if no ECHO)

enum {
    VEOF = 0,
    VINTR,
    VSUSP,
    NCCS
};

#define TERMIOS_DEFAULT \
    { \
        .c_iflag = ICANON, \
        .c_oflag = OPOST, \
        .c_cflag = 0, \
        .c_lflag = ECHO | ECHONL | ECHOE | ECHOK | ISIG, \
        .c_cc = { \
            [VEOF] =    4,  \
            [VINTR] =   3,  \
            [VSUSP] =   26, \
        }, \
    }

typedef unsigned int tcflag_t;
typedef char cc_t;

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
};

#define TCSANOW         0
