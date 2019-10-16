#pragma once

#define isprint(x)  ((x) >= ' ')
#define isdigit(x)  ((x) >= '0' && (x) <= '9')
#define isxdigit(x) (((x) >= 'A' && (x) <= 'F') || \
                     ((x) >= 'a' && (x) <= 'f') || \
                     isdigit(x))
#define isspace(x)  ((x) == ' ' || \
                     (x) == '\t' || \
                     (x) == '\n' || \
                     (x) == '\r')
#define tolower(x)  (((x) >= 'A' && (x) <= 'Z') ? ((x) - 'A' + 'a') : (x))
#define toupper(x)  (((x) >= 'a' && (x) <= 'z') ? ((x) - 'a' + 'A') : (x))
