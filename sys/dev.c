#include "sys/dev.h"
#include "sys/blk.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/string.h"
#include "sys/fs/devfs.h"
#include "sys/fs/vfs.h"
#include "sys/string.h"
#include "sys/fs/fs.h"
#include "sys/attr.h"
#include "sys/heap.h"
#include "sys/errno.h"

static struct vnode *devfs_root = NULL;

static uint64_t dev_count = 0;

int dev_add(enum dev_class cls, int subcls, void *dev, const char *name) {
    char node_name[5];
    struct vnode *node;

    if (!name) {
        // TODO: generate proper names for device nodes
        node_name[4] = 0;
        node_name[3] = '0' + ++dev_count;
        node_name[0] = 'd';
        node_name[1] = 'e';
        node_name[2] = 'v';
        name = node_name;
    }

    // Create a filesystem node for the device
    node = vnode_create(cls == DEV_CLASS_BLOCK ? VN_BLK : VN_CHR, name);
    node->dev = dev;

    // Use inode number to store full device class:subclass
    node->ino = ((uint32_t) cls) | ((uint64_t) subcls << 32);

    node->flags |= VN_MEMORY;

    // If root does not yet exist, make one
    if (!devfs_root) {
        devfs_root = vnode_create(VN_DIR, NULL);
        devfs_root->flags |= VN_MEMORY;
    }

    // TODO: some devices are located in subdirs
    vnode_attach(devfs_root, node);

    kinfo("Created device node: %s\n", node->name);

    vnode_dump_tree(DEBUG_DEFAULT, devfs_root, 0);

    if (cls == DEV_CLASS_BLOCK && subcls < DEV_BLOCK_PART) {
        // Find partitions of block devices
        blk_enumerate_partitions(dev, node);
    }

    return 0;
}

int dev_find(enum dev_class cls, const char *name, struct vnode **node) {
    _assert(node);
    int res;
    if (!devfs_root) {
        return -ENODEV;
    }
    if ((res = vnode_lookup_child(devfs_root, name, node)) != 0) {
        return res;
    }

    if (cls == DEV_CLASS_BLOCK && (*node)->type != VN_BLK) {
        return -ENODEV;
    }
    if (cls == DEV_CLASS_CHAR && (*node)->type != VN_CHR) {
        return -ENODEV;
    }

    return 0;
}
