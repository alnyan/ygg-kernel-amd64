#include "sys/mem/vmalloc.h"
#include "sys/mem/phys.h"
#include "sys/font/psf.h"
#include "sys/display.h"
#include "sys/console.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "user/video.h"
#include "user/errno.h"
#include "user/mman.h"
#include "sys/debug.h"
#include "sys/dev.h"
#include <stddef.h>

int g_display_blink_state = 0;

static LIST_HEAD(displays);

static uint32_t rgb_map[16] = {
    0x000000,
    0x0000AA,
    0x00AA00,
    0x00AAAA,
    0xAA0000,
    0xAA00AA,
    0xAA5500,
    0xAAAAAA,
    0x555555,
    0x5555FF,
    0x55FF55,
    0x55FFFF,
    0xFF5555,
    0xFF55FF,
    0xFFFF55,
    0xFFFFFF,
};

static int display_blk_ioctl(struct blkdev *blk, unsigned long cmd, void *arg) {
    struct display *disp = blk->dev_data;
    _assert(disp);

    switch (cmd) {
    case IOC_GETVMODE:
        _assert(arg);
        ((struct ioc_vmode *) arg)->width = disp->width_pixels;
        ((struct ioc_vmode *) arg)->height = disp->height_pixels;
        ((struct ioc_vmode *) arg)->pitch = disp->pitch;
        ((struct ioc_vmode *) arg)->bpp = disp->bpp;
        return 0;
    default:
        return -EINVAL;
    }
}

static int display_blk_mmap(struct blkdev *blk, uintptr_t base, size_t page_count, int prot, int flags) {
    struct process *proc;
    struct display *disp;
    uintptr_t phys;
    uint64_t pf;

    proc = thread_self->proc;
    _assert(proc);
    disp = blk->dev_data;
    _assert(disp);

    if (page_count > (disp->pitch * disp->height_pixels + MM_PAGE_SIZE - 1) / MM_PAGE_SIZE) {
        return -EINVAL;
    }

    pf = 0;
    if (prot & PROT_WRITE) {
        pf |= MM_PAGE_WRITE;
    }

    // TODO: prevent others (e.g. console) from writing to the memory
    //       while it's mapped

    phys = MM_PHYS(disp->framebuffer);
    for (size_t i = 0; i < page_count; ++i) {
        mm_map_single(proc->space,
                      base + i * MM_PAGE_SIZE,
                      phys + i * MM_PAGE_SIZE,
                      pf | MM_PAGE_USER, PU_DEVICE);
    }

    return 0;
}

struct display *display_get_default(void) {
    if (list_empty(&displays)) {
        return NULL;
    }
    return list_entry(displays.next, struct display, list);
}

void display_add(struct display *disp) {
    // Initialize text mode for framebuffers
    if (disp->flags & DISP_GRAPHIC) {
        disp->width_chars = disp->width_pixels / font->width;
        disp->height_chars = disp->height_pixels / font->height;
    }

    list_add(&disp->list, &displays);
}

void display_postinit(void) {
    char name[4] = "fbX";
    static char cnt = '0';
    struct display *disp;

    list_for_each_entry(disp, &displays, list) {
        if (disp->flags & DISP_LFB) {

            disp->blk.flags = 0;
            disp->blk.dev_data = disp;
            disp->blk.read = NULL;
            disp->blk.write = NULL;
            disp->blk.ioctl = display_blk_ioctl;
            disp->blk.mmap = display_blk_mmap;

            name[2] = cnt++;

            dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_OTHER, &disp->blk, name);
        }
    }
}

void display_setc(struct display *disp, uint16_t y, uint16_t x, uint16_t ch) {
    if (disp->flags & DISP_GRAPHIC) {
        if (disp->flags & DISP_LOCK) {
            return;
        }
        psf_draw(disp, y, x, ch & 0xFF, rgb_map[(ch >> 8) & 0xF], rgb_map[(ch >> 12) & 0xF]);
    } else {
        _assert(disp->setc);
        disp->setc(y, x, ch);
    }
}
