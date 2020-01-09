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

static struct vfs_node *devfs_root_node = NULL;

static struct dev_entry *dev_begin = NULL;
static struct dev_entry *dev_end = NULL;

static uint64_t dev_scsi_bitmap = 0;
static uint64_t dev_ide_bitmap = 0;
static int dev_ram_count = 0;

static int dev_alloc_block_name(uint16_t subclass, char *name) {
    if (subclass == DEV_BLOCK_RAM) {
        strcpy(name, "ram");
        name[3] = dev_ram_count++ + '0';
        name[4] = 0;
        return 0;
    }
    if (subclass == DEV_BLOCK_SDx || subclass == DEV_BLOCK_HDx) {
        uint64_t *bmp = subclass == DEV_BLOCK_SDx ? &dev_scsi_bitmap : &dev_ide_bitmap;
        name[1] = 'd';
        name[0] = subclass == DEV_BLOCK_SDx ? 's' : 'h';

        for (size_t i = 0; i < 64; ++i) {
            if (!(*bmp & (1ULL << i))) {
                *bmp |= 1ULL << i;
                name[2] = 'a' + i;
                name[3] = 0;
                return 0;
            }
        }
    }
    return -1;
}

int dev_alloc_name(enum dev_class cls, uint16_t subclass, char *name) {
    switch (cls) {
    case DEV_CLASS_BLOCK:
        return dev_alloc_block_name(subclass, name);
    default:
        panic("Not implemented\n");
    }
}

static int dev_post_add(struct dev_entry *ent) {
    if (ent->dev_class == DEV_CLASS_BLOCK) {
        struct blkdev *blk = (struct blkdev *) ent->dev;

        blk->ent = ent;

        if (ent->dev_subclass < DEV_BLOCK_PART) {
            if (blk_enumerate_partitions((struct blkdev *) ent->dev) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

void dev_entry_add(struct dev_entry *ent) {
    ent->cdr = NULL;

    if (dev_end) {
        dev_end->cdr = ent;
    } else {
        dev_begin = ent;
    }

    dev_end = ent;

    kdebug("Add device %s\n", ent->dev_name);

    // For example: enumerate partitions on block devices
    _assert(dev_post_add(ent) == 0);
}

struct dev_entry *dev_iter(void) {
    return dev_begin;
}

static int devfs_node_mapper(fs_t *devfs, struct vfs_node **root);
static int devfs_node_stat(vnode_t *node, struct stat *st);

static struct fs_class _devfs = {
    .name = "devfs",
    .opt = FS_NODE_MAPPER,
    .mapper = devfs_node_mapper
};

static struct vnode_operations _devfs_vnode_op = {
    .stat = devfs_node_stat
};

static int devfs_node_stat(vnode_t *node, struct stat *st) {
    // TODO: device modes (permissions)

    st->st_size = 0;
    st->st_blocks = 0;
    st->st_blksize = 0;

    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;

    st->st_dev = 0;
    st->st_rdev = 0;

    st->st_ino = 0;
    st->st_nlink = 1;

    st->st_uid = 0;
    st->st_gid = 0;

    if (node == devfs_root_node->vnode) {
        st->st_mode = S_IFDIR | 0755;
    } else {
        struct dev_entry *ent = node->fs_data;
        _assert(ent);
        if (ent->dev_class == DEV_CLASS_BLOCK) {
            st->st_mode = S_IFBLK | 0600;
        } else {
            st->st_mode = S_IFCHR | 0600;
        }
    }

    return 0;
}

static vnode_t *devfs_create_vnode(fs_t *fs, struct vfs_node *node) {
    vnode_t *res = (vnode_t *) kmalloc(sizeof(vnode_t));
    res->tree_node = node;
    res->fs = fs;
    res->fs_data = NULL;
    res->refcount = 0;
    res->op = &_devfs_vnode_op;
    res->type = VN_DIR;
    return res;
}

static int devfs_node_mapper(fs_t *devfs, struct vfs_node **root) {
    _assert(!devfs_root_node);
    struct vfs_node *_root;
    struct dev_entry *it;

    // Make a root node
    _root = (struct vfs_node *) kmalloc(sizeof(struct vfs_node));
    _root->child = NULL;
    _root->cdr = NULL;
    _root->ismount = 0;
    _root->parent = NULL;
    _root->real_vnode = NULL;
    _root->vnode = NULL;

    // TODO: directory-categories (like "by-uuid" or something)
    for (it = dev_iter(); it; it = it->cdr) {
        struct vfs_node *ent_node = kmalloc(sizeof(struct vfs_node));
        ent_node->child = NULL;
        ent_node->ismount = 0;
        ent_node->real_vnode = NULL;
        strcpy(ent_node->name, it->dev_name);

        // Create a vnode for device
        vnode_t *ent_vnode = devfs_create_vnode(devfs, ent_node);
        if (it->dev_class == DEV_CLASS_BLOCK) {
            ent_vnode->type = VN_BLK;
        } else if (it->dev_class == DEV_CLASS_CHAR) {
            ent_vnode->type = VN_CHR;
        } else {
            panic("Unsupported device class\n");
        }
        ent_vnode->fs_data = it;
        ent_vnode->dev = it->dev;

        ent_node->vnode = ent_vnode;

        ent_node->parent = _root;
        ent_node->cdr = _root->child;
        _root->child = ent_node;
    }

    vnode_t *vnode = devfs_create_vnode(devfs, _root);
    _root->vnode = vnode;

    *root = _root;
    devfs_root_node = _root;

    return 0;
}

static __init void devfs_init(void) {
    fs_class_register(&_devfs);
}
