#include "user/netctl.h"
#include "sys/assert.h"
#include "user/errno.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "net/util.h"
#include "net/if.h"

static struct netdev *g_netdev;
static int g_last_eth = 0;

struct netdev *netdev_create(int type) {
    struct netdev *net = kmalloc(sizeof(struct netdev));
    _assert(net);

    net->inaddr = 0;
    net->flags = 0;
    net->type = type;
    net->arp_ent_head = NULL;

    switch (type) {
    case IF_T_ETH:
        strcpy(net->name, "ethN");
        net->name[3] = '0' + g_last_eth++;
        break;
    default:
        panic("Unhandled network device type: %u\n", type);
    }

    net->next = g_netdev;
    g_netdev = net;

    kinfo("New netdev: %s\n", net->name);

    return net;
}

struct netdev *netdev_by_name(const char *name) {
    for (struct netdev *dev = g_netdev; dev; dev = dev->next) {
        if (!strcmp(dev->name, name)) {
            return dev;
        }
    }

    return NULL;
}

int netctl(struct netdev *dev, uint32_t op, void *data) {
    _assert(dev);

    switch (op) {
    case NETCTL_SET_INADDR:
        // Have to flush old addr first
        if (dev->flags & IF_F_HASIP) {
            return -EEXIST;
        }
        _assert(data);
        dev->inaddr = *(uint32_t *) data;
        dev->flags |= IF_F_HASIP;
        kinfo("%s: set address: " FMT_INADDR "\n", dev->name, VA_INADDR(dev->inaddr));
        return 0;
    default:
        kwarn("%s: invalid operation requested: 0x%x\n", dev->name, op);
        return -EINVAL;
    }
}
