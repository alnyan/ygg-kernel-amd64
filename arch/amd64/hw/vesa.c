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

void vesa_early_init(struct boot_video_info *info) {
    if (info->width && info->height) {
        // If these fields are set, framebuffer is available
        display_vesa_fb.flags = DISP_GRAPHIC | DISP_LFB;
        display_vesa_fb.width_pixels = info->width;
        display_vesa_fb.height_pixels = info->height;
        display_vesa_fb.pitch = info->pitch;
        display_vesa_fb.bpp = info->bpp;
        display_vesa_fb.framebuffer = MM_VIRTUALIZE(info->framebuffer_phys);

        list_head_init(&display_vesa_fb.list);
    }
}

// Initialize early (pre-PCI) framebuffer
void vesa_add_display(void) {
    display_add(&display_vesa_fb);
}
