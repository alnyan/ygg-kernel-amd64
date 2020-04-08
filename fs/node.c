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
    for (int i = 0; i < depth; ++i) {
        debugs(level, "  ");
    }

    if (node->flags & VN_MEMORY) {
        debugc(level, 'M');
    }

    switch (node->type) {
    case VN_DIR:
        debugs(level, "\033[33m");
        break;
    case VN_REG:
        debugs(level, "\033[1m");
        break;
    case VN_MNT:
        debugs(level, "\033[35m");
        break;
    case VN_LNK:
        debugs(level, "\033[36m");
        break;
    case VN_BLK:
    case VN_CHR:
        debugs(level, "\033[32m");
        break;
    default:
        debugs(level, "\033[41m");
        break;
    }

    if (node->name[0]) {
        debugs(level, node->name);
    } else {
        debugs(level, "[root]");
    }

    if (node->type == VN_MNT) {
        _assert(node->target);
        debugs(level, " => [root]");
        node = node->target;
    }
    while (node && node->type == VN_LNK) {
        if (node->target) {
            debugs(level, "\033[0m -> ");
            switch (node->target->type) {
            case VN_DIR:
                debugs(level, "\033[33m");
                break;
            case VN_REG:
                debugs(level, "\033[1m");
                break;
            case VN_MNT:
                debugs(level, "\033[35m");
                break;
            case VN_LNK:
                debugs(level, "\033[36m");
                break;
            case VN_BLK:
            case VN_CHR:
                debugs(level, "\033[32m");
                break;
            default:
                debugs(level, "\033[41m");
                break;
            }
            debugs(level, node->target->name);
        } else {
            debugs(level, "\033[0m -> \033[41m???");
        }

        node = node->target;
    }

    debugs(level, "\033[0m");

    if (node && node->first_child) {
        // Only dirs have children
        _assert(node->type == VN_DIR);
        debugs(level, " {\n");

        for (struct vnode *ch = node->first_child; ch; ch = ch->next_child) {
            vnode_dump_tree(level, ch, depth + 1);
        }

        for (int i = 0; i < depth; ++i) {
            debugs(level, "  ");
        }
        debugs(level, "}\n");
    } else {
        debugc(level, '\n');
    }
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
