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

static char sdx_last = 'a';
static char hdx_last = 'a';
static uint64_t dev_count = 0;

static int dev_create_name(enum dev_class cls, int subcls, char *name) {
    if (cls == DEV_CLASS_BLOCK) {
        switch (subcls) {
        case DEV_BLOCK_SDx:
            strcpy(name, "sdx");
            name[2] = sdx_last++;
            return 0;
        }
    }
    return -1;
}

int dev_add(enum dev_class cls, int subcls, void *dev, const char *name) {
    char node_name[32];
    struct vnode *node;

    if (!name) {
        if (dev_create_name(cls, subcls, node_name) != 0) {
            return -1;
        }

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

    kdebug("Created device node: %s\n", node->name);

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
