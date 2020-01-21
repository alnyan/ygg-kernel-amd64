#include "sys/amd64/hw/vesa.h"
#include "sys/amd64/mm/map.h"
#include "sys/fbdev.h"
#include "sys/errno.h"
#include "sys/amd64/cpu.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/vmalloc.h"
#include "sys/blk.h"
#include "sys/mman.h"
#include "sys/string.h"
#include "sys/panic.h"
#include "sys/debug.h"
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

static int vesa_blk_ioctl(struct blkdev *blk, unsigned long cmd, void *arg) {
    switch (cmd) {
    case FB_IOC_GETSZ:
        _assert(arg);
        ((uint32_t *) arg)[0] = vesa_width;
        ((uint32_t *) arg)[1] = vesa_height;
        ((uint32_t *) arg)[2] = vesa_pitch;
        ((uint32_t *) arg)[3] = vesa_bpp;
        return 0;
    case FB_IOC_HOLD:
        if (vesa_hold) {
            return -EACCES;
        }
        vesa_hold = 1;
        return 0;
    case FB_IOC_UNHOLD:
        vesa_hold = 0;
        return 0;
    default:
        return -EINVAL;
    }
}

static void *vesa_blk_mmap(struct blkdev *blk, struct ofile *fd, void *hint, size_t length, int flags) {
    struct thread *thr = get_cpu()->thread;
    _assert(thr);

    size_t this_length = vesa_pitch * vesa_height;
    _assert(length >= this_length);

    size_t npages = (length + 0xFFF) / 0x1000;

    // Ignore hint
    uintptr_t addr = vmfind(thr->space, 0x40000000, 0xF0000000, npages);
    _assert(addr != MM_NADDR);

    // Map the pages
    for (size_t i = 0; i < npages; ++i) {
        _assert(amd64_map_single(thr->space, i * 0x1000 + addr, i * 0x1000 + MM_PHYS(vesa_addr), (1 << 2) | (1 << 1)) == 0);
        //int amd64_map_single(mm_space_t pml4, uintptr_t virt_addr, uintptr_t phys, uint32_t flags);
    }

    return (void *) addr;
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
