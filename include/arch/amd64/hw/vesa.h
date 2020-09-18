#pragma once
#include "sys/types.h"

//struct multiboot_tag_framebuffer;
struct display;

struct boot_video_info {
    // TODO: bgr/rgb?
    uint32_t width, height, bpp;
    size_t pitch;
    uintptr_t framebuffer_phys;
};

struct display *vesa_get_display(void);
void vesa_early_init(struct boot_video_info *info);
void vesa_add_display(void);
void vesa_put(uint32_t x, uint32_t y, uint32_t v);
void vesa_clear(uint32_t color);
