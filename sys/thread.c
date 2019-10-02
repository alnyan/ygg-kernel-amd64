#include "sys/thread.h"
#include "sys/mem.h"

int thread_info_init(thread_info_t *info) {
    info->flags = 0;
    info->parent = 0;
    info->pid = 0;
    info->space = 0;
    info->status = 0;

    memset(info->fds, 0, sizeof(info->fds));

    info->ioctx.cwd_vnode = NULL;
    info->ioctx.uid = 0;
    info->ioctx.gid = 0;

    return 0;
}
