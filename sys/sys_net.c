#include "sys/sys_net.h"
#include "sys/assert.h"
#include "user/errno.h"
#include "net/if.h"

int sys_netctl(const char *name, uint32_t op, void *arg) {
    _assert(name);

    struct netdev *dev = netdev_by_name(name);
    if (!dev) {
        return -ENODEV;
    }

    return netctl(dev, op, arg);
}
