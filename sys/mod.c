#include "user/errno.h"
#include "fs/sysfs.h"

int sys_module_unload(const char *name) {
    return -ENOSYS;
}

int sys_module_load(const char *path, const char *params) {

    return -ENOSYS;
}

int mod_list(void *ctx, char *buf, size_t lim) {
    return -ENOSYS;
}
