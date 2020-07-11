#include "user/errno.h"
#include "sys/block/blk.h"
#include "fs/devfs.h"
#include "fs/node.h"
#include "fs/vfs.h"
#include "sys/string.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "fs/fs.h"
#include "sys/panic.h"
#include "sys/debug.h"
#include "sys/heap.h"
#include "sys/attr.h"
#include "sys/dev.h"

static struct vnode *devfs_root = NULL;
static int devfs_init(struct fs *fs, const char *opt);
static struct vnode *devfs_get_root(struct fs *fs);
static int devfs_vnode_stat(struct vnode *node, struct stat *st);

static char cdx_last = 'a';
static char sdx_last = 'a';
static char hdx_last = 'a';
static uint64_t dev_count = 0;

static struct fs_class _devfs = {
    .name = "devfs",
    .opt = 0,
    .init = devfs_init,
    .get_root = devfs_get_root
};

static struct vnode_operations _devfs_node_ops = {
    .stat = devfs_vnode_stat
};

/////

static int dev_create_name(enum dev_class cls, int subcls, char *name) {
    if (cls == DEV_CLASS_BLOCK) {
        switch (subcls) {
        case DEV_BLOCK_SDx:
            strcpy(name, "sdx");
            name[2] = sdx_last++;
            return 0;
        case DEV_BLOCK_CDx:
            strcpy(name, "cdx");
            name[2] = cdx_last++;
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
        devfs_root->mode = 0555;
        devfs_root->uid = 0;
        devfs_root->gid = 0;
        devfs_root->op = &_devfs_node_ops;
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

int dev_add_live_link(const char *name, vnode_link_getter_t getter) {
    struct vnode *node = vnode_create(VN_LNK, name);
    node->target_func = getter;
    node->flags |= VN_MEMORY | VN_PER_PROCESS;

    devfs_ensure_root();

    node->mode = 0777;
    node->uid = 0;
    node->gid = 0;
    node->op = &_devfs_node_ops;

    vnode_attach(devfs_root, node);

    return 0;
}

int dev_add_link(const char *name, struct vnode *to) {
    struct vnode *node = vnode_create(VN_LNK, name);
    node->target = to;
    node->flags |= VN_MEMORY;

    devfs_ensure_root();

    node->mode = 0777;
    node->uid = 0;
    node->gid = 0;
    node->op = &_devfs_node_ops;

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
    node->op = &_devfs_node_ops;

    // Default permissions for devices
    node->mode = 0600;
    node->uid = 0;
    node->gid = 0;

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

////

static int devfs_vnode_stat(struct vnode *node, struct stat *st) {
    st->st_mode = (node->mode & VFS_MODE_MASK) | vfs_vnode_to_mode(node->type);
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_size = 0;
    st->st_blksize = 0;
    st->st_blocks = 0;
    st->st_ino = 0;

    st->st_atime = system_boot_time;
    st->st_mtime = system_boot_time;
    st->st_ctime = system_boot_time;

    st->st_dev = 0;
    st->st_rdev = 0;

    return 0;
}

////

__init(devfs_class_init) {
    fs_class_register(&_devfs);
}
