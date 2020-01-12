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
static int devfs_init(struct fs *fs, const char *opt);
static struct vnode *devfs_get_root(struct fs *fs);

static char sdx_last = 'a';
static char hdx_last = 'a';
static uint64_t dev_count = 0;

static struct fs_class _devfs = {
    .name = "devfs",
    .opt = 0,
    .init = devfs_init,
    .get_root = devfs_get_root
};

/////

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

static void devfs_ensure_root(void) {
    // If root does not yet exist, make one
    if (!devfs_root) {
        devfs_root = vnode_create(VN_DIR, NULL);
        devfs_root->flags |= VN_MEMORY;
    }
}

/////

static int devfs_init(struct fs *fs, const char *opt) {
    return 0;
}

static struct vnode *devfs_get_root(struct fs *fs) {
    return devfs_root;
}

/////

int dev_add_link(const char *name, struct vnode *to) {
    struct vnode *node = vnode_create(VN_LNK, name);
    node->target = to;
    node->flags |= VN_MEMORY;

    devfs_ensure_root();

    vnode_attach(devfs_root, node);

    return 0;
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

    devfs_ensure_root();

    // TODO: some devices are located in subdirs
    vnode_attach(devfs_root, node);

    kdebug("Created device node: %s\n", node->name);

    if (cls == DEV_CLASS_BLOCK && subcls < DEV_BLOCK_PART) {
        // Find partitions of block devices
        blk_enumerate_partitions(dev, node);
    }

    return 0;
}

int dev_find(enum dev_class cls, const char *name, struct vnode **node) {
    struct vnode *e;
    int res;

    _assert(node);

    if (!devfs_root) {
        return -ENODEV;
    }

    if ((res = vnode_lookup_child(devfs_root, name, &e)) != 0) {
        return res;
    }

    if (e->type == VN_LNK) {
        _assert(e->target);
        e = e->target;
    }

    if (cls == DEV_CLASS_BLOCK && e->type != VN_BLK) {
        return -ENODEV;
    }
    if (cls == DEV_CLASS_CHAR && e->type != VN_CHR) {
        return -ENODEV;
    }

    *node = e;

    return 0;
}

static void __init devfs_class_init(void) {
    fs_class_register(&_devfs);
}
