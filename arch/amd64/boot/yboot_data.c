#include "arch/amd64/boot/yboot.h"

char yboot_memory_map_data[32768];

struct yb_proto_v1 yboot_data __attribute__((section(".data.boot, \"aw\", %progbits //"))) = {
    .hdr = {
        .kernel_magic = YB_KERNEL_MAGIC_V1A
    },
    .flags = 0
#if defined(VESA_ENABLE)
        | YB_FLAG_VIDEO
#endif
        | YB_FLAG_INITRD
        | YB_FLAG_UPPER,

    .memory_map = {
        .address = (uint64_t) yboot_memory_map_data - 0xFFFFFF0000000000,
        .size = sizeof(yboot_memory_map_data)
    },
#if defined(VESA_ENABLE)
    .video = {
        .width = VESA_WIDTH,
        .height = VESA_HEIGHT,
        .format = YB_LFB_BGR32,
    },
#endif
};
