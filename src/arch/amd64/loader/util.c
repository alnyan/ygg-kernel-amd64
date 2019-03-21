#include "util.h"

void *memset(void *blk, int v, size_t sz) {
    for (size_t i = 0; i < sz; ++i) {
        ((char *) blk)[i] = v;
    }
    return blk;
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

void panic(const char *s) {
    memset((void *) 0xB8000, 0, 80 * 25 * 2);

    for (size_t i = 0; *s; ++i, ++s) {
        ((uint16_t *) 0xB8000)[i] = *s | 0x700;
    }

    while (1) {
        asm volatile ("cli; hlt");
    }
}
