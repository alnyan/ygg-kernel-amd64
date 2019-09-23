#include "sys/thread.h"

int thread_info_init(thread_info_t *info) {
    info->flags = 0;
    info->parent = 0;
    info->pid = 0;
    info->space = 0;
    info->status = 0;

    return 0;
}
