#include "user/errno.h"
#include "fs/node.h"
#include "fs/vfs.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "fs/fs.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/heap.h"
#include "sys/mem/slab.h"

static struct slab_cache *vnode_cache = NULL;

struct vnode *vnode_create(enum vnode_type t, const char *name) {
    if (!vnode_cache) {
        vnode_cache = slab_cache_get(sizeof(struct vnode));
        kdebug("Initialized vnode cache\n");
    }
    struct vnode *node = slab_calloc(vnode_cache);
    _assert(node);

    node->type = t;
    if (name) {
        _assert(strlen(name) < NODE_MAXLEN);
        strcpy(node->name, name);
    }

    return node;
}

void vnode_destroy(struct vnode *vn) {
    _assert(vnode_cache);
    _assert(!vn->open_count);
    slab_free(vnode_cache, vn);
}

void vnode_attach(struct vnode *parent, struct vnode *child) {
    _assert(parent);
    _assert(child);
    _assert(!child->parent);

    child->parent = parent;
    child->next_child = parent->first_child;
    parent->first_child = child;
}

void vnode_detach(struct vnode *node) {
    _assert(node);

    struct vnode *parent = node->parent;
    node->parent = NULL;

    if (!parent) {
        return;
    }

    if (node == parent->first_child) {
        parent->first_child = node->next_child;
        node->next_child = NULL;
        return;
    }

    for (struct vnode *ch = parent->first_child; ch; ch = ch->next_child) {
        if (ch->next_child == node) {
            ch->next_child = node->next_child;
            node->next_child = NULL;
            return;
        }
    }

    panic("Parent has no link to its child node\n");
}

// Only dumps cached nodes in filesystem tree
void vnode_dump_tree(int level, struct vnode *node, int depth) {
    // XXX: I'll reimplement this with proper symlink support
}

// NOTE: Only looks up in-memory nodes, does not perform any operations to fetch
// the nodes from storage
// NOTE: Resolution of .. and . is performed when resolving paths, as a node can
// have multiple parents (roots of filesystems mounted at different locations)
int vnode_lookup_child(struct vnode *of, const char *name, struct vnode **child) {
    _assert(of);
    _assert(child);
    _assert(name);
    _assert(strlen(name) < NODE_MAXLEN);

    for (struct vnode *ch = of->first_child; ch; ch = ch->next_child) {
        if (!strcmp(ch->name, name)) {
            *child = ch;
            return 0;
        }
    }

    return -ENOENT;
}
