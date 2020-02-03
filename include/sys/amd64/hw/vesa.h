#pragma once
#include "sys/amd64/loader/multiboot.h"
#include "sys/types.h"

extern int vesa_available, vesa_hold;
extern uint32_t vesa_width, vesa_height, vesa_pitch, vesa_bpp;
extern uint64_t vesa_addr;

void amd64_vesa_init(multiboot_info_t *info);
void vesa_put(uint32_t x, uint32_t y, uint32_t v);
void vesa_clear(uint32_t color);
