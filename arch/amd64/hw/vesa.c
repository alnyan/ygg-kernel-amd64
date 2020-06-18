#include "arch/amd64/multiboot2.h"
#include "arch/amd64/hw/vesa.h"
#include "sys/display.h"
#include "sys/debug.h"
#include "sys/mm.h"

static struct display display_vesa_fb;

void amd64_vesa_init(struct multiboot_tag_framebuffer *tag) {
    if (tag->common.framebuffer_type == 1) {
        //vesa_available = 1;
        //vesa_addr =     MM_VIRTUALIZE(tag->common.framebuffer_addr);
        //vesa_pitch =    tag->common.framebuffer_pitch;
        //vesa_width =    tag->common.framebuffer_width;
        //vesa_height =   tag->common.framebuffer_height;
        //vesa_bpp =      tag->common.framebuffer_bpp;
        kdebug("Initializing VESA framebuffer\n");
        display_vesa_fb.flags = DISP_GRAPHIC | DISP_LFB;
        display_vesa_fb.width_pixels = tag->common.framebuffer_width;
        display_vesa_fb.height_pixels = tag->common.framebuffer_height;
        display_vesa_fb.bpp = tag->common.framebuffer_bpp;
        display_vesa_fb.pitch = tag->common.framebuffer_pitch;
        display_vesa_fb.framebuffer = MM_VIRTUALIZE(tag->common.framebuffer_addr);
        list_head_init(&display_vesa_fb.list);

        display_add(&display_vesa_fb);

        //kdebug("Set up VESA video:\n");
        //kdebug("BPP: %d, Width: %d, Height: %d\n", vesa_bpp, vesa_width, vesa_height);
        //kdebug("Addr: %p\n", vesa_addr);

        //dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_OTHER, &_vesa_fb0, "fb0");
    }
}
