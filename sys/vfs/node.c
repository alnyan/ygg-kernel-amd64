#include "sys/fs/node.h"
#include "sys/fs/vfs.h"
#include "sys/fs/fs.h"
#include "sys/errno.h"

#include "sys/debug.h"
#include "sys/assert.h"
#include "sys/panic.h"
#include "sys/string.h"
#include "sys/heap.h"

static const char *path_element(const char *path, char *element) {
    const char *sep = strchr(path, '/');
    if (!sep) {
        strcpy(element, path);
        return NULL;
    } else {
        _assert(sep - path < NODE_MAXLEN);
        strncpy(element, path, sep - path);
        element[sep - path] = 0;

        while (*sep == '/') {
            ++sep;
        }
        if (!*sep) {
            return NULL;
        }

        return sep;
    }
}

struct vnode *vnode_create(enum vnode_type t, const char *name) {
    struct vnode *node = (struct vnode *) kmalloc(sizeof(struct vnode));
    _assert(node);

    node->type = t;
    node->flags = 0;

    node->parent = NULL;
    node->first_child = NULL;
    node->next_child = NULL;

    node->fs = NULL;
    node->fs_data = NULL;

    node->op = NULL;
    node->dev = NULL;

    if (name) {
        _assert(strlen(name) < NODE_MAXLEN);
        strcpy(node->name, name);
    } else {
        node->name[0] = 0;
    }

    return node;
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
    switch (node->type) {
    case VN_DIR:
        debugs(level, "\033[33m");
        break;
    case VN_REG:
        debugs(level, "\033[1m");
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
    debugs(level, "\033[0m");

    if (node->first_child) {
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

int vnode_lookup_tree(struct vnode *at, const char *path, struct vnode **child) {
    char name[NODE_MAXLEN];
    const char *child_path;
    struct vnode *node;
    int err;

    _assert(at);
    _assert(child);

    if (!path || !*path) {
        *child = at;
        kdebug("empty hit-final %s\n", at->name);
        return 0;
    }

    // The paths are always assumed to be relative
    while (*path == '/') {
        ++path;
    }

    while (1) {
        child_path = path_element(path, name);

        if (!strcmp(name, ".")) {
            // Refers to this node
            kdebug(". hit %s\n", at->name);
            continue;
        } else if (!strcmp(name, "..")) {
            // Refers to node's parent
            // And this won't work for filesystem mountpoints:
            // Path like a/mnt/.. would still resolve the same point as
            // a/mnt
            struct vnode *parent = at->parent;
            if (!parent) {
                parent = at;
            }
            kdebug(".. hit %s\n", parent->name);
            return vnode_lookup_tree(parent, child_path, child);
        }

        break;
    }

    // TODO: lookup_or_load
    if ((err = vnode_lookup_child(at, name, &node)) != 0) {
        kdebug("miss %s\n", name);
        return err;
    }

    _assert(node);

    if (!child_path) {
        kdebug("hit-final %s\n", name);
        *child = node;
        return 0;
    } else {
        kdebug("hit %s\n", name);
        return vnode_lookup_tree(node, child_path, child);
    }
}
