#include "arch/amd64/hw/vesa.h"
#include "arch/amd64/mm/map.h"
#include "sys/block/blk.h"
#include "arch/amd64/cpu.h"
#include "sys/vmalloc.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/dev.h"
#include "sys/mm.h"

//static int vesa_blk_ioctl(struct blkdev *blk, unsigned long cmd, void *arg);
//static void *vesa_blk_mmap(struct blkdev *blk, struct ofile *fd, void *hint, size_t length, int flags);

int vesa_hold = 0;
int vesa_available = 0;
uint64_t vesa_addr;
uint32_t vesa_bpp, vesa_pitch, vesa_width, vesa_height;

static struct blkdev _vesa_fb0 = {
    .flags = 0,
    .dev_data = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = NULL,
    .mmap = NULL
};

void vesa_clear(uint32_t color) {
    if (vesa_hold) {
        return;
    }
    switch (vesa_bpp) {
    case 32:
        for (size_t i = 0; i < vesa_height; ++i) {
            memsetl((uint32_t *) (i * vesa_pitch + vesa_addr), color, vesa_width);
        }
        break;
    default:
        panic("NYI\n");
    }
}

void vesa_put(uint32_t x, uint32_t y, uint32_t v) {
    if (vesa_hold) {
        return;
    }
    switch (vesa_bpp) {
    case 32:
        ((uint32_t *) (vesa_addr + vesa_pitch * y))[x] = v;
        break;
    default:
        panic("NYI\n");
    }
}

void amd64_vesa_init(multiboot_info_t *info) {
    if (info->flags & (1 << 12)) {
        vesa_available = 1;
        vesa_addr = MM_VIRTUALIZE(info->framebuffer_addr);
        vesa_pitch = info->framebuffer_pitch;
        vesa_width = info->framebuffer_width;
        vesa_height = info->framebuffer_height;
        vesa_bpp = info->framebuffer_bpp;

        if (info->framebuffer_type == 0) {
            panic("Indexed framebuffers are not supported yet\n");
        } else {
            kdebug("Set up VESA video:\n");
            kdebug("BPP: %d, Width: %d, Height: %d\n", vesa_bpp, vesa_width, vesa_height);
            kdebug("Addr: %p\n", vesa_addr);
        }

        dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_OTHER, &_vesa_fb0, "fb0");
    }
}
