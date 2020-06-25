#pragma once
#include "sys/types.h"

struct multiboot_tag_framebuffer;
struct display;

struct display *vesa_get_display(void);
void vesa_init(struct multiboot_tag_framebuffer *info);
void vesa_put(uint32_t x, uint32_t y, uint32_t v);
void vesa_clear(uint32_t color);
