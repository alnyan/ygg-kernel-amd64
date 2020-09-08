#include "arch/amd64/boot/yboot.h"

char yboot_memory_map_data[32768];

struct yboot_v1 yboot_data __attribute__((section(".data.boot, \"aw\", %progbits //"))) = {
    .header = {
        .kernel_magic = YB_KERNEL_MAGIC_V1
    },

#if defined(VESA_ENABLE)
    .video_width = VESA_WIDTH,
    .video_height = VESA_HEIGHT,
    .video_format = YB_VIDEO_FORMAT_BGR32,
#endif

    .memory_map_data = (uint64_t) yboot_memory_map_data - 0xFFFFFF0000000000,
    .memory_map_size = sizeof(yboot_memory_map_data)
};
