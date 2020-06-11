#include "arch/amd64/multiboot2.h"
#include "arch/amd64/hw/vesa.h"
#include "sys/mem/vmalloc.h"
#include "arch/amd64/cpu.h"
#include "sys/block/blk.h"
#include "sys/thread.h"
#include "user/video.h"
#include "user/errno.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/dev.h"
#include "sys/mm.h"

static int vesa_blk_ioctl(struct blkdev *blk, unsigned long cmd, void *arg);
static void *vesa_blk_mmap(struct blkdev *blk, struct ofile *fd, void *hint, size_t length, int flags);

int vesa_hold = 0;
int vesa_available = 0;
uint64_t vesa_addr;
uint32_t vesa_bpp, vesa_pitch, vesa_width, vesa_height;

static struct blkdev _vesa_fb0 = {
    .flags = 0,
    .dev_data = NULL,
    .read = NULL,
    .write = NULL,
    .ioctl = vesa_blk_ioctl,
    .mmap = vesa_blk_mmap
};

static int vesa_blk_ioctl(struct blkdev *blk, unsigned long cmd, void *arg) {
    switch (cmd) {
    case IOC_GETVMODE:
        _assert(arg);
        ((struct ioc_vmode *) arg)->width = vesa_width;
        ((struct ioc_vmode *) arg)->height = vesa_height;
        return 0;
    case IOC_FBCON:
        _assert(arg);
        vesa_hold = !*(uint32_t *) arg;
        return 0;
    default:
        return -EINVAL;
    }
}

static void *vesa_blk_mmap(struct blkdev *blk, struct ofile *fd, void *hint, size_t length, int flags) {
    struct thread *thr = thread_self;
    _assert(thr);
    uintptr_t vaddr = vmfind(thr->space, 0x80000000, 0x100000000, length / MM_PAGE_SIZE);
    _assert(vaddr != MM_NADDR);
    uintptr_t phys = MM_PHYS(vesa_addr);

    for (size_t i = 0; i < length / MM_PAGE_SIZE; ++i) {
        mm_map_single(thr->space, vaddr + i * MM_PAGE_SIZE, phys + i * MM_PAGE_SIZE, MM_PAGE_WRITE | MM_PAGE_USER, 0);
    }

    return (void *) vaddr;
}

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

void amd64_vesa_init(struct multiboot_tag_framebuffer *tag) {
    if (tag->common.framebuffer_type == 1) {
        vesa_available = 1;
        vesa_addr =     MM_VIRTUALIZE(tag->common.framebuffer_addr);
        vesa_pitch =    tag->common.framebuffer_pitch;
        vesa_width =    tag->common.framebuffer_width;
        vesa_height =   tag->common.framebuffer_height;
        vesa_bpp =      tag->common.framebuffer_bpp;

        kdebug("Set up VESA video:\n");
        kdebug("BPP: %d, Width: %d, Height: %d\n", vesa_bpp, vesa_width, vesa_height);
        kdebug("Addr: %p\n", vesa_addr);

        dev_add(DEV_CLASS_BLOCK, DEV_BLOCK_OTHER, &_vesa_fb0, "fb0");
    }
}
