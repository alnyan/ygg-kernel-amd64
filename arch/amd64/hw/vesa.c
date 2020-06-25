#include "arch/amd64/multiboot2.h"
#include "arch/amd64/hw/vesa.h"
#include "sys/display.h"
#include "sys/debug.h"
#include "sys/mm.h"

static struct display display_vesa_fb = {0};

struct display *vesa_get_display(void) {
    if (!display_vesa_fb.flags) {
        // VESA framebuffer isn't found
        return NULL;
    } else {
        return &display_vesa_fb;
    }
}

// Initialize early (pre-PCI) framebuffer
void vesa_init(struct multiboot_tag_framebuffer *tag) {
    if (tag->common.framebuffer_type == 1) {
        display_vesa_fb.flags = DISP_GRAPHIC | DISP_LFB;
        display_vesa_fb.width_pixels = tag->common.framebuffer_width;
        display_vesa_fb.height_pixels = tag->common.framebuffer_height;
        display_vesa_fb.bpp = tag->common.framebuffer_bpp;
        display_vesa_fb.pitch = tag->common.framebuffer_pitch;
        display_vesa_fb.framebuffer = MM_VIRTUALIZE(tag->common.framebuffer_addr);
        list_head_init(&display_vesa_fb.list);

        display_add(&display_vesa_fb);
    }
}
