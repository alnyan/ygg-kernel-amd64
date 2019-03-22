#include "util.h"

void *memset(void *blk, int v, size_t sz) {
    for (size_t i = 0; i < sz; ++i) {
        ((char *) blk)[i] = v;
    }
    return blk;
}

void *memcpy(void *dst, const void *src, size_t sz) {
    for (size_t i = 0; i < sz; ++i) {
        ((char *) dst)[i] = ((const char *) src)[i];
    }
    return dst;
}

int strcmp(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b) {
        if (*a != *b) {
            return 1;
        }
    }
    return *a != *b;
}

static inline void outb(uint16_t addr, uint8_t v) {
    asm volatile("outb %0, %1"::"a"(v), "Nd"(addr));
}

static inline uint8_t inb(uint16_t addr) {
	uint8_t v;
    asm volatile("inb %1, %0":"=a"(v):"Nd"(addr));
	return v;
}

void putc(char c) {
    while (!(inb(0x3F8 + 5) & 0x20)) {}
	outb(0x3F8, c);
}

void puts(const char *s) {
    for (; *s; ++s) {
        putc(*s);
    }
}

void putx(uint64_t v) {
    static const char *set = "0123456789abcdef";
    char buf[32];
    memset(buf, 0, sizeof(buf));
    if (!v) {
        puts("0x0");
        return;
    }

    int c = 0;

    while (v) {
        buf[c++] = set[v & 0xF];
        v >>= 4;
    }

    buf[c] = 0;

    for (int i = 0, j = c - 1; i < j; ++i, --j) {
        buf[i] ^= buf[j];
        buf[j] ^= buf[i];
        buf[i] ^= buf[j];
    }

    puts("0x");
    puts(buf);
}

void panic(const char *s) {
    memset((void *) 0xB8000, 0, 80 * 25 * 2);

    for (size_t i = 0; *s; ++i, ++s) {
        ((uint16_t *) 0xB8000)[i] = *s | 0x700;
    }

    while (1) {
        asm volatile ("cli; hlt");
    }
}
