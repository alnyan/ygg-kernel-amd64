#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "net/if.h"

static struct netdev *g_netdev;
static int g_last_eth = 0;

struct netdev *netdev_create(int type) {
    struct netdev *net = kmalloc(sizeof(struct netdev));
    _assert(net);

    net->inaddr = 0;
    net->flags = 0;
    net->type = type;

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
