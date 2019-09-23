#pragma once
#include <stdint.h>

static inline void outb(uint16_t addr, uint8_t v) {
    asm volatile("outb %0, %1"::"a"(v), "Nd"(addr));
}

static inline uint8_t inb(uint16_t addr) {
	uint8_t v;
    asm volatile("inb %1, %0":"=a"(v):"Nd"(addr));
	return v;
}

static inline void outl(uint16_t addr, uint32_t v) {
    asm volatile("outl %0, %1"::"a"(v), "Nd"(addr));
}

static inline uint32_t inl(uint16_t addr) {
	uint32_t v;
    asm volatile("inl %1, %0":"=a"(v):"Nd"(addr));
	return v;
}

static inline void io_wait(void) {
    asm volatile("jmp 1f\n\t"
                 "1:jmp 2f\n\t"
                 "2:" );
}
