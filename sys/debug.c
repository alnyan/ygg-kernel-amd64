#include "sys/string.h"
#include "sys/debug.h"
#include "sys/ctype.h"
#include "sys/attr.h"
#include "sys/spin.h"
#include "sys/config.h"
#include "sys/elf.h"
#include "sys/assert.h"
#include <stdint.h>

#if defined(ARCH_AMD64)
#include "arch/amd64/hw/rs232.h"
#include "arch/amd64/hw/con.h"
#endif

////

static uintptr_t g_symtab_ptr = 0;
static uintptr_t g_strtab_ptr = 0;
static size_t g_symtab_size = 0;
static size_t g_strtab_size = 0;

void debug_symbol_table_set(uintptr_t s0, uintptr_t s1, size_t z0, size_t z1) {
    g_symtab_ptr = s0;
    g_symtab_size = z0;
    g_strtab_ptr = s1;
    g_strtab_size = z1;
}

int debug_symbol_find_by_name(const char *name, uintptr_t *value) {
    _assert(g_symtab_ptr && g_strtab_ptr);
    Elf64_Sym *symtab = (Elf64_Sym *) g_symtab_ptr;
    for (size_t i = 0; i < g_symtab_size / sizeof(Elf64_Sym); ++i) {
        Elf64_Sym *sym = &symtab[i];
        if (sym->st_name < g_strtab_size) {
            const char *r_name = (const char *) g_strtab_ptr + sym->st_name;

            if (!strcmp(r_name, name)) {
                *value = sym->st_value;
                return 0;
            }
        }
    }

    return -1;
}

int debug_symbol_find(uintptr_t addr, const char **name, uintptr_t *base) {
    if (g_symtab_ptr && g_strtab_ptr) {
        size_t offset = 0;
        Elf64_Sym *sym;

        while (offset < g_symtab_size) {
            sym = (Elf64_Sym *) (g_symtab_ptr + offset);

            if (ELF_ST_TYPE(sym->st_info) == STT_FUNC) {
                if (sym->st_value <= addr && sym->st_value + sym->st_size >= addr) {
                    *base = sym->st_value;
                    if (sym->st_name < g_strtab_size) {
                        *name = (const char *) (g_strtab_ptr + sym->st_name);
                    } else {
                        *name = "<invalid>";
                    }
                    return 0;
                }
            }

            offset += sizeof(Elf64_Sym);
        }
    }

    return -1;
}

void debug_backtrace(uintptr_t rbp, int depth, int limit) {
    // Typical layout:
    // rbp + 08 == rip
    // rbp + 00 == rbp_1

    if (!limit) {
        return;
    }

    if ((rbp & 0xFFFFFF0000000000) != 0xFFFFFF0000000000) {
        kfatal("-- %rbp is not from kernel space\n");
        return;
    }

    uintptr_t rip =      ((uintptr_t *) rbp)[1];
    uintptr_t rbp_next = ((uintptr_t *) rbp)[0];

    uintptr_t base;
    const char *name;

    if (debug_symbol_find(rip, &name, &base) == 0) {
        kfatal("%d: %p <%s + %04x>\n", depth, rip, name, rip - base);
    } else {
        kfatal("%d: %p (unknown)\n", depth, rip);
    }

    if (rbp_next == 0) {
        kfatal("-- End of frame chain\n");
        return;
    }

    debug_backtrace(rbp_next, depth + 1, limit - 1);
}

////

static const char *s_debug_xs_set0 = "0123456789abcdef";
static const char *s_debug_xs_set1 = "0123456789ABCDEF";

// Don't know if it's a best idea, but at least guarantees
// non-overlapping debug lines
static spin_t debug_spin = 0;

void fmtsiz(char *out, size_t sz) {
    static const char sizs[] = "KMGTPE???";
    size_t f = sz, r = 0;
    int pwr = 0;
    size_t l = 0;

    while (f >= 1536) {
        r = ((f % 1024) * 10) / 1024;
        f /= 1024;
        ++pwr;
    }

    debug_ds(f, out, 0, 0);
    l = strlen(out);

    if (pwr) {
        out[l++] = '.';
        out[l++] = '0' + r;

        out[l++] = sizs[pwr - 1];

        out[l++] = 'i';
    }

    out[l++] = 'B';
    out[l++] = 0;
}

void debugc(int level, char c) {
#if defined(ARCH_AMD64)
    if (DEBUG_SERIAL(level) & kernel_config[CFG_DEBUG]) {
        rs232_send(RS232_COM1, c);
    }
    if (DEBUG_DISP(level) & kernel_config[CFG_DEBUG]) {
        amd64_con_putc(c);
    }
#endif
}

void debugs(int level, const char *s) {
    char c;
    while ((c = *(s++))) {
        debugc(level, c);
    }
}

static void debugspl(int level, const char *s, char p, size_t c) {
    size_t l = strlen(s);
    for (size_t i = l; i < c; ++i) {
        debugc(level, p);
    }
    debugs(level, s);
}

static void debugspr(int level, const char *s, char p, size_t c) {
    size_t l = strlen(s);
    debugs(level, s);
    for (size_t i = l; i < c; ++i) {
        debugc(level, p);
    }
}

static void debugsp(int level, const char *s, char padc, int padl) {
    if (padl > 0) {
        debugspl(level, s, padc, padl);
    } else {
        debugspr(level, s, padc, -padl);
    }
}

void debug_ds(int64_t x, char *res, int s, int sz) {
    if (!x) {
        res[0] = '0';
        res[1] = 0;
        return;
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
}

void debug_xs(uint64_t v, char *res, const char *set) {
    if (!v) {
        res[0] = '0';
        res[1] = 0;
        return;
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
}

void debugf(int level, const char *f, ...) {
    va_list args;
    va_start(args, f);
    debugfv(level, f, args);
    va_end(args);
}

void debugfv(int level, const char *fmt, va_list args) {
    uintptr_t irq;
    spin_lock_irqsave(&debug_spin, &irq);

    char c;
    union {
        const char *v_string;
        char v_char;
        int32_t v_int32;
        uint32_t v_uint32;
        int64_t v_int64;
        uint64_t v_uint64;
        uintptr_t v_ptr;
    } value;
    char buf[64];
    char padc;
    int padn;
    int padd;

    while ((c = *fmt)) {
        switch (c) {
            case '%':
                c = *(++fmt);

                padc = ' ';
                padd = 1;
                padn = 0;
                if (c == '0') {
                    padc = c;
                }

                if (c == '-') {
                    padd = -1;
                    c = *(++fmt);
                }

                while (c >= '0' && c <= '9') {
                    padn *= 10;
                    padn += padd * (int) (c - '0');
                    c = *(++fmt);
                }

                switch (c) {
                    case 'l':
                        c = *(++fmt);
                        switch (c) {
                            case 'd':
                                value.v_int64 = va_arg(args, int64_t);
                                debug_ds(value.v_int64, buf, 1, 1);
                                debugsp(level, buf, padc, padn);
                                break;
                            case 'u':
                                value.v_uint64 = va_arg(args, uint64_t);
                                debug_ds(value.v_uint64, buf, 0, 1);
                                debugsp(level, buf, padc, padn);
                                break;
                            case 'x':
                                value.v_uint64 = va_arg(args, uint64_t);
                                debug_xs(value.v_uint64, buf, s_debug_xs_set0);
                                debugsp(level, buf, padc, padn);
                                break;
                            case 'X':
                                value.v_uint64 = va_arg(args, uint64_t);
                                debug_xs(value.v_uint64, buf, s_debug_xs_set1);
                                debugsp(level, buf, padc, padn);
                                break;
                            case 'p':
                                value.v_uint64 = va_arg(args, uint64_t);
                                debugc(level, '0');
                                debugc(level, 'x');
                                debug_xs(value.v_uint64, buf, s_debug_xs_set0);
                                debugspl(level, buf, '0', sizeof(uint64_t) * 2);
                                break;
                            default:
                                debugc(level, '%');
                                debugc(level, 'l');
                                debugc(level, c);
                                break;
                        }
                        break;
                    case 'c':
                        // char is promoted to int
                        value.v_char = va_arg(args, int);
                        debugc(level, value.v_char);
                        break;
                    case 'd':
                        value.v_int64 = va_arg(args, int32_t);
                        debug_ds(value.v_int64 & 0xFFFFFFFF, buf, 1, 0);
                        debugsp(level, buf, padc, padn);
                        break;
                    case 'u':
                        value.v_uint64 = va_arg(args, uint32_t);
                        debug_ds(value.v_uint64 & 0xFFFFFFFF, buf, 0, 0);
                        debugsp(level, buf, padc, padn);
                        break;
                    case 'x':
                        value.v_uint64 = va_arg(args, uint32_t);
                        debug_xs(value.v_uint64 & 0xFFFFFFFF, buf, s_debug_xs_set0);
                        debugsp(level, buf, padc, padn);
                        break;
                    case 'X':
                        value.v_uint64 = va_arg(args, uint32_t);
                        debug_xs(value.v_uint64 & 0xFFFFFFFF, buf, s_debug_xs_set1);
                        debugsp(level, buf, padc, padn);
                        break;
                    case 'p':
                        value.v_ptr = va_arg(args, uintptr_t);
                        debugc(level, '0');
                        debugc(level, 'x');
                        debug_xs(value.v_ptr, buf, s_debug_xs_set0);
                        debugspl(level, buf, '0', sizeof(uintptr_t) * 2);
                        break;
                    case 'S':
                        value.v_ptr = va_arg(args, uintptr_t);
                        fmtsiz(buf, value.v_ptr);
                        debugsp(level, buf, padc, padn);
                        break;
                    case 's':
                        value.v_string = va_arg(args, const char *);
                        debugsp(level, value.v_string ? value.v_string : "(null)", padc, padn);
                        break;
                    case '%':
                        debugc(level, '%');
                        break;
                    default:
                        debugc(level, '%');
                        debugc(level, c);
                        break;
                }
                break;
            default:
                debugc(level, c);
                break;
        }

        ++fmt;
    }

    spin_release_irqrestore(&debug_spin, &irq);
}

void debug_dump(int level, const void *block, size_t count) {
    size_t n_lines = (count + 15) / 16;
    const char *bytes = block;

    for (size_t l = 0; l < n_lines; ++l) {
        debugf(level, "%08X: ", l * 16);

        for (size_t i = 0; i < 8; ++i) {
            if (i * 2 + l * 16 < count) {
                uint16_t word = *(const uint16_t *) (bytes + i * 2 + l * 16);

                debugc(level, s_debug_xs_set1[(word >> 4) & 0xF]);
                debugc(level, s_debug_xs_set1[word & 0xF]);

                debugc(level, s_debug_xs_set1[word >> 12]);
                debugc(level, s_debug_xs_set1[(word >> 8) & 0xF]);
            } else {
                debugc(level, ' ');
                debugc(level, ' ');
                debugc(level, ' ');
                debugc(level, ' ');
            }
            debugc(level, ' ');
        }

        debugc(level, '|');
        debugc(level, ' ');
        for (size_t i = 0; i < 16; ++i) {
            if (i + l * 16 < count) {
                char c = bytes[i + l * 16];

                if (isprint(c)) {
                    debugc(level, c);
                } else if (c != 0) {
                    debugc(level, ' ');
                } else {
                    debugc(level, '.');
                }
            }
        }
        debugc(level, '\n');
    }
}

uintptr_t __stack_chk_guard = 0x8BAD1DEA6EA11743;
__attribute__((noreturn)) void __stack_chk_fail(void) {
    panic("Stack smashing detected\n");
}
