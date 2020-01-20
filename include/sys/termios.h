#pragma once

#define TIOCSPGRP       0x5410
#define TIOCGWINSZ      0x5413

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};
