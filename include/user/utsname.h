#pragma once

struct utsname {
    char sysname[16];
    char nodename[8];
    char release[16];
    char version[32];
    char machine[16];
    char domainname[64];
};
