#include "sys/fs/node.h"
#include "sys/fs/vfs.h"
#include "sys/fs/fs.h"

#include "sys/debug.h"
#include "sys/assert.h"
#include "sys/heap.h"
#include "sys/string.h"

static void vfs_node_remove(struct vfs_node *node) {
    // If node->parent == NULL, we've called this
    // function on [root], which should not be possible
    if (!node || !node->parent) {
        return;
    }

    struct vfs_node *parent = node->parent;

    if (parent->child == node) {
        parent->child = node->cdr;
    } else {
        for (struct vfs_node *it = parent->child; it; it = it->cdr) {
            if (it->cdr == node) {
                it->cdr = node->cdr;
                break;
            }
        }

        // TODO: handle case when for some reason node is absent in
        //       parent's children list
    }

    // Decrement refcount for parent, unless it's [root]
    if (parent->parent && !parent->child && !parent->vnode->refcount) {
        vnode_free(parent->vnode);
    }

    // Just a wrapper around free()
    vfs_node_free(node);
}

void vnode_free(vnode_t *vn) {
    _assert(vn && vn->op);
    _assert(!vn->refcount);
    struct vfs_node *node = vn->tree_node;
    struct vfs_node *link_node = NULL;

    if (node->ismount) {
        return;
    }

    if (vn->op->destroy) {
        // This will free/release underlying fs_data
        vn->op->destroy(vn);
    }

    if (node) {
        vfs_node_remove(node);
    }

    memset(vn, 0, sizeof(vnode_t));
    kfree(vn);
}

void vnode_ref(vnode_t *vn) {
//    char buf[1024];
//    vfs_vnode_path(buf, vn);
//    printf("++refcount for %s\n", buf);
    _assert(vn);
    _assert(vn->fs);
    _assert(vn->fs->cls);

    // FS_NODE_MAPPER means persistent vnodes
    if (vn->fs->cls->opt & FS_NODE_MAPPER) {
        return;
    }

    struct vfs_node *node = (struct vfs_node *) vn->tree_node;
    if (node && !node->parent) {
        // Don't change refcounter for root nodes
        return;
    }

    ++vn->refcount;
}

void vnode_unref(vnode_t *vn) {
    _assert(vn);
    _assert(vn->fs);
    _assert(vn->fs->cls);

    // FS_NODE_MAPPER means persistent vnodes
    if (vn->fs->cls->opt & FS_NODE_MAPPER) {
        return;
    }

    // TODO: don't free root nodes
    char buf[1024];
    vfs_vnode_path(buf, vn);
    struct vfs_node *node = (struct vfs_node *) vn->tree_node;
    if (!node->parent) {
        return;
    }

    if (vn->refcount > 0) {
        --vn->refcount;

        if (vn->refcount == 0) {
            if (node->child) {
                return;
            }

            kdebug("free %s\n", buf);
            // Free vnode
            vnode_free(vn);
        }
    } else {
        _assert(node->child);

        kdebug("--refcount with 0 but child\n");
    }
}
