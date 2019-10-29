#include "bits/printf.h"
#include <string.h>
#include <stdint.h>

union printfv {
    int value_int;
    unsigned int value_uint;
    long value_long;
    uint32_t value_uint32;
    uint64_t value_uint64;
    uintptr_t value_ptr;
    const char *value_str;
};

static const char *s_print_xs_set0 = "0123456789abcdef";
static const char *s_print_xs_set1 = "0123456789ABCDEF";

static int vsnprintf_ds(int64_t x, char *res, int s, int sz) {
    if (!x) {
        res[0] = '0';
        res[1] = 0;
        return 1;
    }

    int c;
    uint64_t v;

    if (sz) {
        if (s && x < 0) {
            v = (uint64_t) -x;
        } else {
            s = 0;
            v = (uint64_t) x;
        }
    } else {
        if (s && ((int32_t) x) < 0) {
            v = (uint64_t) -((int32_t) x);
        } else {
            s = 0;
            v = (uint64_t) x;
        }
    }

    c = 0;

    while (v) {
        res[c++] = '0' + v % 10;
        v /= 10;
    }

    if (s) {
        res[c++] = '-';
    }

    res[c] = 0;

    for (int i = 0, j = c - 1; i < j; ++i, --j) {
        res[i] ^= res[j];
        res[j] ^= res[i];
        res[i] ^= res[j];
    }

    return c;
}

static int vsnprintf_xs(uint64_t v, char *res, const char *set) {
    if (!v) {
        res[0] = '0';
        res[1] = 0;
        return 1;
    }

    int c = 0;

    while (v) {
        res[c++] = set[v & 0xF];
        v >>= 4;
    }

    res[c] = 0;

    for (int i = 0, j = c - 1; i < j; ++i, --j) {
        res[i] ^= res[j];
        res[j] ^= res[i];
        res[i] ^= res[j];
    }

    return c;
}



int vsnprintf(char *buf, size_t len, const char *fmt, va_list args) {
    union printfv val;
    char cbuf[64];
    size_t p = 0;
    // padn > 0:
    //  --------vvvvv
    // padn < 0:
    // vvvv----------
    int padn;
    char padc;
    int pads;
    int clen;
    char c;

#define __putc(ch) \
    if (p == len - 1) { \
        goto __end; \
    } else { \
        buf[p++] = ch; \
    }

#define __puts(s, l) \
    if (padn >= 0) { \
        if (l < padn) { \
            size_t padd = padn - l; \
            if (p + padd < len + 1) { \
                memset(buf + p, padc, padd); \
                p += padd; \
            } else { \
                memset(buf + p, padc, len - 1 - p); \
                p = len - 1; \
                goto __end; \
            } \
        } \
        if (p + l < len + 1) { \
            strncpy(buf + p, s, l); \
            p += l; \
        } else { \
            strncpy(buf + p, s, len - 1 - p); \
            p = len - 1; \
            goto __end; \
        } \
    } else { \
        padn = -padn; \
        if (p + l < len + 1) { \
            strncpy(buf + p, s, l); \
            p += l; \
        } else { \
            strncpy(buf + p, s, len - 1 - p); \
            p = len - 1; \
            goto __end; \
        } \
        if (l < padn) { \
            size_t padd = padn - l; \
            if (p + padd < len + 1) { \
                memset(buf + p, padc, padd); \
                p += padd; \
            } else { \
                memset(buf + p, padc, len - 1 - p); \
                p = len - 1; \
                goto __end; \
            } \
        } \
    }

    while ((c = *fmt)) {
        switch (c) {
        case '%':
            c = *++fmt;
            padn = 0;
            padc = ' ';
            pads = 1;

            if (c == '0') {
                padc = c;
                c = *++fmt;
            }
            if (c == '-') {
                pads = -1;
                c = *++fmt;
            }

            while (c >= '0' && c <= '9') {
                padn *= 10;
                padn += c - '0';
                c = *++fmt;
            }

            padn *= pads;

            switch (c) {
            case 'l':
                // Not supported
                return -1;

            case 's':
                if ((val.value_str = va_arg(args, const char *))) {
                    __puts(val.value_str, strlen(val.value_str));
                } else {
                    __puts("(null)", 6);
                }
                break;

            case 'p':
                val.value_ptr = va_arg(args, uintptr_t);
                __putc('0');
                __putc('x');
                padn = sizeof(uintptr_t) * 2;
                padc = '0';
                clen = vsnprintf_xs(val.value_ptr, cbuf, s_print_xs_set0);
                __puts(cbuf, clen);
                break;

            case 'i':
            case 'd':
                val.value_int = va_arg(args, int);
                clen = vsnprintf_ds(val.value_int, cbuf, 1, 1);
                __puts(cbuf, clen);
                break;
            case 'u':
                val.value_uint = va_arg(args, unsigned int);
                clen = vsnprintf_ds(val.value_uint, cbuf, 0, 1);
                __puts(cbuf, clen);
                break;

            case 'x':
                val.value_uint32 = va_arg(args, uint32_t);
                clen = vsnprintf_xs(val.value_uint32, cbuf, s_print_xs_set0);
                __puts(cbuf, clen);
                break;
            case 'X':
                val.value_uint32 = va_arg(args, uint32_t);
                clen = vsnprintf_xs(val.value_uint32, cbuf, s_print_xs_set1);
                __puts(cbuf, clen);
                break;

            case 'c':
                val.value_int = va_arg(args, int);
                __putc(val.value_int);
                break;

            default:
                __putc('%');
                __putc(c);
                break;
            }

            break;
        default:
            __putc(c);
            break;
        }

        ++fmt;
    }

#undef __puts
#undef __putc

__end:
    buf[p] = 0;
    return p;
}
