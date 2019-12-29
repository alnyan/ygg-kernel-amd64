#include "sys/fs/devfs.h"
#include "sys/fs/vfs.h"
#include "sys/string.h"
#include "sys/fs/fs.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/attr.h"
#include "sys/heap.h"

static int devfs_node_mapper(fs_t *devfs, struct vfs_node **root);

static struct fs_class _devfs = {
    .name = "devfs",
    .opt = FS_NODE_MAPPER,
    .mapper = devfs_node_mapper
};

static struct vnode_operations _devfs_vnode_op = {
};

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

// TODO: handle devices added post-mapper

static int devfs_node_mapper(fs_t *devfs, struct vfs_node **root) {
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

    return 0;
}

static __init void devfs_init(void) {
    fs_class_register(&_devfs);
}
